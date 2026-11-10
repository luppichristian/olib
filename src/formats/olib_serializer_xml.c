/*
MIT License

Copyright (c) 2026 Christian Luppi

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include <olib/olib_formats.h>
#include "text_parsing_utilities.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <math.h>

// #############################################################################
// Context structure for XML serialization
// #############################################################################

// Structure to hold parsed tag info
typedef struct {
  char tag_name[64];
  char type_attr[32];
  size_t name_attr_pos;
  bool has_name_attr;
  char dims_attr[128];
  bool is_self_closing;
  bool is_closing_tag;
} xml_tag_info_t;

// Container tag type for tracking what closing tag to use
typedef enum {
  XML_CONTAINER_STANDALONE,  // <list> or <struct>
  XML_CONTAINER_KEY,         // <key name="..." type="list/struct">
  XML_CONTAINER_ITEM         // <item type="list/struct">
} xml_container_type_t;

typedef struct {
  // Write mode
  char* write_buffer;
  size_t write_capacity;
  size_t write_size;
  int indent_level;
  bool in_list;
  bool list_first_item;
  bool in_struct;
  bool struct_first_item;
  const char* pending_key;
  bool needs_root_close;

  // Track container types for proper closing tags (stack indexed by indent level)
  xml_container_type_t container_stack[32];
  bool parent_in_list[32];
  bool parent_in_struct[32];
  bool parent_list_first[32];
  bool parent_struct_first[32];
  int container_depth;

  // Read mode (using shared parsing utilities)
  text_parse_ctx_t parse;
  olib_object_type_t pending_value_type;  // Type from key/item tag attribute
  bool has_pending_type;                   // Whether we have a pending type
  xml_tag_info_t pending_tag_info;         // Complete tag info for matrix dims
} xml_ctx_t;

// #############################################################################
// Write helpers
// #############################################################################

static bool xml_ensure_write_capacity(xml_ctx_t* ctx, size_t needed) {
  size_t required = ctx->write_size + needed;
  if (required <= ctx->write_capacity) {
    return true;
  }

  size_t new_capacity = ctx->write_capacity ? ctx->write_capacity * 2 : 256;
  while (new_capacity < required) {
    new_capacity *= 2;
  }

  char* new_buffer = olib_realloc(ctx->write_buffer, new_capacity);
  if (!new_buffer) {
    return false;
  }

  ctx->write_buffer = new_buffer;
  ctx->write_capacity = new_capacity;
  return true;
}

static bool xml_write_str(xml_ctx_t* ctx, const char* str) {
  size_t len = strlen(str);
  if (!xml_ensure_write_capacity(ctx, len)) return false;
  memcpy(ctx->write_buffer + ctx->write_size, str, len);
  ctx->write_size += len;
  return true;
}

static bool xml_write_char(xml_ctx_t* ctx, char c) {
  if (!xml_ensure_write_capacity(ctx, 1)) return false;
  ctx->write_buffer[ctx->write_size++] = c;
  return true;
}

static bool xml_write_indent(xml_ctx_t* ctx) {
  for (int i = 0; i < ctx->indent_level; i++) {
    if (!xml_write_str(ctx, "  ")) return false;
  }
  return true;
}

static bool xml_valid_character(uint32_t code) {
  return code == 0x09 || code == 0x0a || code == 0x0d ||
         (code >= 0x20 && code <= 0xd7ff) ||
         (code >= 0xe000 && code <= 0xfffd) ||
         (code >= 0x10000 && code <= 0x10ffff);
}

// XML 1.0 Char production on a UTF-8 encoded buffer. Check the whole input,
// including attributes and text, so invalid bytes are never emitted or read.
static bool xml_valid_characters(const uint8_t* data, size_t size) {
  for (size_t pos = 0; pos < size;) {
    uint32_t code = data[pos];
    uint32_t minimum = 0;
    size_t length = 1;
    if (code < 0x80) {
      // ASCII character.
    } else if (code >= 0xc2 && code <= 0xdf) {
      code &= 0x1f;
      minimum = 0x80;
      length = 2;
    } else if (code >= 0xe0 && code <= 0xef) {
      code &= 0x0f;
      minimum = 0x800;
      length = 3;
    } else if (code >= 0xf0 && code <= 0xf4) {
      code &= 0x07;
      minimum = 0x10000;
      length = 4;
    } else {
      return false;
    }
    if (length > size - pos) return false;
    for (size_t i = 1; i < length; ++i) {
      uint8_t byte = data[pos + i];
      if ((byte & 0xc0) != 0x80) return false;
      code = (code << 6) | (byte & 0x3f);
    }
    if (code < minimum || !xml_valid_character(code)) {
      return false;
    }
    pos += length;
  }
  return true;
}

// Write XML-escaped string content (not the quotes, just the content)
static bool xml_write_escaped(xml_ctx_t* ctx, const char* str, bool attribute) {
  if (!str) return true;
  if (!xml_valid_characters((const uint8_t*)str, strlen(str))) return false;
  for (const char* p = str; *p; p++) {
    switch (*p) {
      case '&':
        if (!xml_write_str(ctx, "&amp;")) return false;
        break;
      case '<':
        if (!xml_write_str(ctx, "&lt;")) return false;
        break;
      case '>':
        if (!xml_write_str(ctx, "&gt;")) return false;
        break;
      case '"':
        if (!xml_write_str(ctx, "&quot;")) return false;
        break;
      case '\'':
        if (!xml_write_str(ctx, "&apos;")) return false;
        break;
      case '\r':
        if (!xml_write_str(ctx, "&#13;")) return false;
        break;
      case '\t':
        if (attribute) {
          if (!xml_write_str(ctx, "&#9;")) return false;
        } else if (!xml_write_char(ctx, *p)) return false;
        break;
      case '\n':
        if (attribute) {
          if (!xml_write_str(ctx, "&#10;")) return false;
        } else if (!xml_write_char(ctx, *p)) return false;
        break;
      default:
        if (!xml_write_char(ctx, *p)) return false;
        break;
    }
  }
  return true;
}

static bool xml_write_newline_indent(xml_ctx_t* ctx) {
  if (!xml_write_char(ctx, '\n')) return false;
  return xml_write_indent(ctx);
}

// Write opening tag like <tagname> or <tagname attr="value">
static bool xml_write_open_tag(xml_ctx_t* ctx, const char* tag) {
  if (!xml_write_char(ctx, '<')) return false;
  if (!xml_write_str(ctx, tag)) return false;
  if (!xml_write_char(ctx, '>')) return false;
  return true;
}

// Write closing tag like </tagname>
static bool xml_write_close_tag(xml_ctx_t* ctx, const char* tag) {
  if (!xml_write_str(ctx, "</")) return false;
  if (!xml_write_str(ctx, tag)) return false;
  if (!xml_write_char(ctx, '>')) return false;
  return true;
}

// Write a complete element like <tagname>value</tagname>
static bool xml_write_element(xml_ctx_t* ctx, const char* tag, const char* value) {
  if (!xml_write_open_tag(ctx, tag)) return false;
  if (!xml_write_escaped(ctx, value, false)) return false;
  if (!xml_write_close_tag(ctx, tag)) return false;
  return true;
}

// Handle struct key prefix for values
static bool xml_write_struct_value_begin(xml_ctx_t* ctx, const char* type_tag) {
  if (ctx->in_struct && ctx->pending_key) {
    // Inside struct: <key name="field" type="int">value</key>
    if (!xml_write_str(ctx, "<key name=\"")) return false;
    if (!xml_write_escaped(ctx, ctx->pending_key, true)) return false;
    if (!xml_write_str(ctx, "\" type=\"")) return false;
    if (!xml_write_str(ctx, type_tag)) return false;
    if (!xml_write_str(ctx, "\">")) return false;
    ctx->pending_key = NULL;
    return true;
  } else if (ctx->in_list) {
    // Inside list: <item type="int">value</item>
    if (!xml_write_str(ctx, "<item type=\"")) return false;
    if (!xml_write_str(ctx, type_tag)) return false;
    if (!xml_write_str(ctx, "\">")) return false;
    return true;
  } else {
    // Top-level: <int>value</int>
    if (!xml_write_open_tag(ctx, type_tag)) return false;
    return true;
  }
}

static bool xml_write_struct_value_end(xml_ctx_t* ctx, const char* type_tag) {
  if (ctx->in_struct) {
    // Close </key>
    if (!xml_write_str(ctx, "</key>")) return false;
  } else if (ctx->in_list) {
    // Close </item>
    if (!xml_write_str(ctx, "</item>")) return false;
  } else {
    // Close </type_tag>
    if (!xml_write_close_tag(ctx, type_tag)) return false;
  }
  return true;
}

// #############################################################################
// Write callbacks
// #############################################################################

static bool xml_write_int(void* ctx, int64_t value) {
  xml_ctx_t* c = (xml_ctx_t*)ctx;

  if (c->in_list) {
    if (!c->list_first_item) {
      if (!xml_write_char(c, '\n')) return false;
    }
    c->list_first_item = false;
    if (!xml_write_indent(c)) return false;
  } else if (c->in_struct) {
    if (!c->struct_first_item) {
      if (!xml_write_char(c, '\n')) return false;
    }
    c->struct_first_item = false;
    if (!xml_write_indent(c)) return false;
  }

  if (!xml_write_struct_value_begin(c, "int")) return false;

  char buf[32];
  snprintf(buf, sizeof(buf), "%lld", (long long)value);
  if (!xml_write_str(c, buf)) return false;

  if (!xml_write_struct_value_end(c, "int")) return false;
  return true;
}

static bool xml_write_uint(void* ctx, uint64_t value) {
  xml_ctx_t* c = (xml_ctx_t*)ctx;

  if (c->in_list) {
    if (!c->list_first_item) {
      if (!xml_write_char(c, '\n')) return false;
    }
    c->list_first_item = false;
    if (!xml_write_indent(c)) return false;
  } else if (c->in_struct) {
    if (!c->struct_first_item) {
      if (!xml_write_char(c, '\n')) return false;
    }
    c->struct_first_item = false;
    if (!xml_write_indent(c)) return false;
  }

  if (!xml_write_struct_value_begin(c, "uint")) return false;

  char buf[32];
  snprintf(buf, sizeof(buf), "%llu", (unsigned long long)value);
  if (!xml_write_str(c, buf)) return false;

  if (!xml_write_struct_value_end(c, "uint")) return false;
  return true;
}

static bool xml_write_float(void* ctx, double value) {
  xml_ctx_t* c = (xml_ctx_t*)ctx;

  if (c->in_list) {
    if (!c->list_first_item) {
      if (!xml_write_char(c, '\n')) return false;
    }
    c->list_first_item = false;
    if (!xml_write_indent(c)) return false;
  } else if (c->in_struct) {
    if (!c->struct_first_item) {
      if (!xml_write_char(c, '\n')) return false;
    }
    c->struct_first_item = false;
    if (!xml_write_indent(c)) return false;
  }

  if (!xml_write_struct_value_begin(c, "float")) return false;

  char buf[64];
  snprintf(buf, sizeof(buf), "%.17g", value);
  if (!xml_write_str(c, buf)) return false;

  if (!xml_write_struct_value_end(c, "float")) return false;
  return true;
}

static bool xml_write_string(void* ctx, const char* value) {
  xml_ctx_t* c = (xml_ctx_t*)ctx;

  if (c->in_list) {
    if (!c->list_first_item) {
      if (!xml_write_char(c, '\n')) return false;
    }
    c->list_first_item = false;
    if (!xml_write_indent(c)) return false;
  } else if (c->in_struct) {
    if (!c->struct_first_item) {
      if (!xml_write_char(c, '\n')) return false;
    }
    c->struct_first_item = false;
    if (!xml_write_indent(c)) return false;
  }

  if (!xml_write_struct_value_begin(c, "string")) return false;
  if (!xml_write_escaped(c, value, false)) return false;
  if (!xml_write_struct_value_end(c, "string")) return false;
  return true;
}

static bool xml_write_bool(void* ctx, bool value) {
  xml_ctx_t* c = (xml_ctx_t*)ctx;

  if (c->in_list) {
    if (!c->list_first_item) {
      if (!xml_write_char(c, '\n')) return false;
    }
    c->list_first_item = false;
    if (!xml_write_indent(c)) return false;
  } else if (c->in_struct) {
    if (!c->struct_first_item) {
      if (!xml_write_char(c, '\n')) return false;
    }
    c->struct_first_item = false;
    if (!xml_write_indent(c)) return false;
  }

  if (!xml_write_struct_value_begin(c, "bool")) return false;
  if (!xml_write_str(c, value ? "true" : "false")) return false;
  if (!xml_write_struct_value_end(c, "bool")) return false;
  return true;
}

static bool xml_write_list_begin(void* ctx, size_t size) {
  (void)size;
  xml_ctx_t* c = (xml_ctx_t*)ctx;
  if (c->container_depth >= 32) return false;

  if (c->in_list) {
    if (!c->list_first_item) {
      if (!xml_write_char(c, '\n')) return false;
    }
    c->list_first_item = false;
    if (!xml_write_indent(c)) return false;
  } else if (c->in_struct) {
    if (!c->struct_first_item) {
      if (!xml_write_char(c, '\n')) return false;
    }
    c->struct_first_item = false;
    if (!xml_write_indent(c)) return false;
  }

  // Track what type of container tag we're using
  xml_container_type_t container_type;

  // Handle struct key for nested containers
  if (c->in_struct && c->pending_key) {
    if (!xml_write_str(c, "<key name=\"")) return false;
    if (!xml_write_escaped(c, c->pending_key, true)) return false;
    if (!xml_write_str(c, "\" type=\"list\">")) return false;
    c->pending_key = NULL;
    container_type = XML_CONTAINER_KEY;
  } else if (c->in_list) {
    if (!xml_write_str(c, "<item type=\"list\">")) return false;
    container_type = XML_CONTAINER_ITEM;
  } else {
    if (!xml_write_str(c, "<list>")) return false;
    container_type = XML_CONTAINER_STANDALONE;
  }

  // Preserve the parent context while visiting the nested container.
  int depth = c->container_depth++;
  c->container_stack[depth] = container_type;
  c->parent_in_list[depth] = c->in_list;
  c->parent_in_struct[depth] = c->in_struct;
  c->parent_list_first[depth] = c->list_first_item;
  c->parent_struct_first[depth] = c->struct_first_item;

  if (!xml_write_char(c, '\n')) return false;
  c->indent_level++;
  c->in_list = true;
  c->in_struct = false;
  c->list_first_item = true;
  return true;
}

static bool xml_write_list_end(void* ctx) {
  xml_ctx_t* c = (xml_ctx_t*)ctx;
  c->indent_level--;
  if (!xml_write_char(c, '\n')) return false;
  if (!xml_write_indent(c)) return false;

  // Pop container type and write matching closing tag
  xml_container_type_t container_type = XML_CONTAINER_STANDALONE;
  if (c->container_depth > 0) {
    container_type = c->container_stack[--c->container_depth];
  }

  switch (container_type) {
    case XML_CONTAINER_KEY:
      if (!xml_write_str(c, "</key>")) return false;
      break;
    case XML_CONTAINER_ITEM:
      if (!xml_write_str(c, "</item>")) return false;
      break;
    case XML_CONTAINER_STANDALONE:
    default:
      if (!xml_write_str(c, "</list>")) return false;
      break;
  }

  c->in_list = c->parent_in_list[c->container_depth];
  c->in_struct = c->parent_in_struct[c->container_depth];
  c->list_first_item = c->parent_list_first[c->container_depth];
  c->struct_first_item = c->parent_struct_first[c->container_depth];

  return true;
}

static bool xml_write_struct_begin(void* ctx) {
  xml_ctx_t* c = (xml_ctx_t*)ctx;
  if (c->container_depth >= 32) return false;

  if (c->in_list) {
    if (!c->list_first_item) {
      if (!xml_write_char(c, '\n')) return false;
    }
    c->list_first_item = false;
    if (!xml_write_indent(c)) return false;
  } else if (c->in_struct) {
    if (!c->struct_first_item) {
      if (!xml_write_char(c, '\n')) return false;
    }
    c->struct_first_item = false;
    if (!xml_write_indent(c)) return false;
  }

  // Track what type of container tag we're using
  xml_container_type_t container_type;

  // Handle struct key for nested containers
  if (c->in_struct && c->pending_key) {
    if (!xml_write_str(c, "<key name=\"")) return false;
    if (!xml_write_escaped(c, c->pending_key, true)) return false;
    if (!xml_write_str(c, "\" type=\"struct\">")) return false;
    c->pending_key = NULL;
    container_type = XML_CONTAINER_KEY;
  } else if (c->in_list) {
    if (!xml_write_str(c, "<item type=\"struct\">")) return false;
    container_type = XML_CONTAINER_ITEM;
  } else {
    if (!xml_write_str(c, "<struct>")) return false;
    container_type = XML_CONTAINER_STANDALONE;
  }

  // Preserve the parent context while visiting the nested container.
  int depth = c->container_depth++;
  c->container_stack[depth] = container_type;
  c->parent_in_list[depth] = c->in_list;
  c->parent_in_struct[depth] = c->in_struct;
  c->parent_list_first[depth] = c->list_first_item;
  c->parent_struct_first[depth] = c->struct_first_item;

  if (!xml_write_char(c, '\n')) return false;
  c->indent_level++;
  c->in_list = false;
  c->in_struct = true;
  c->struct_first_item = true;
  return true;
}

static bool xml_write_struct_key(void* ctx, const char* key) {
  xml_ctx_t* c = (xml_ctx_t*)ctx;
  c->pending_key = key;
  return true;
}

static bool xml_write_struct_end(void* ctx) {
  xml_ctx_t* c = (xml_ctx_t*)ctx;
  c->indent_level--;
  if (!xml_write_char(c, '\n')) return false;
  if (!xml_write_indent(c)) return false;

  // Pop container type and write matching closing tag
  xml_container_type_t container_type = XML_CONTAINER_STANDALONE;
  if (c->container_depth > 0) {
    container_type = c->container_stack[--c->container_depth];
  }

  switch (container_type) {
    case XML_CONTAINER_KEY:
      if (!xml_write_str(c, "</key>")) return false;
      break;
    case XML_CONTAINER_ITEM:
      if (!xml_write_str(c, "</item>")) return false;
      break;
    case XML_CONTAINER_STANDALONE:
    default:
      if (!xml_write_str(c, "</struct>")) return false;
      break;
  }

  c->in_list = c->parent_in_list[c->container_depth];
  c->in_struct = c->parent_in_struct[c->container_depth];
  c->list_first_item = c->parent_list_first[c->container_depth];
  c->struct_first_item = c->parent_struct_first[c->container_depth];
  return true;
}

// #############################################################################
// XML parsing helpers
// #############################################################################

// Skip XML whitespace
static void xml_parse_skip_whitespace(text_parse_ctx_t* p) {
  while (p->pos < p->size) {
    char c = p->buffer[p->pos];
    if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
      p->pos++;
    } else {
      break;
    }
  }
}

// Skip XML comments <!-- ... -->
static void xml_parse_skip_comments(text_parse_ctx_t* p) {
  while (p->pos + 4 <= p->size && strncmp(p->buffer + p->pos, "<!--", 4) == 0) {
    p->pos += 4;
    while (p->pos + 3 <= p->size) {
      if (strncmp(p->buffer + p->pos, "-->", 3) == 0) {
        p->pos += 3;
        break;
      }
      p->pos++;
    }
    xml_parse_skip_whitespace(p);
  }
}

// Skip XML declaration <?xml ... ?>
static void xml_parse_skip_declaration(text_parse_ctx_t* p) {
  xml_parse_skip_whitespace(p);
  if (p->pos + 5 <= p->size && strncmp(p->buffer + p->pos, "<?xml", 5) == 0) {
    while (p->pos + 2 <= p->size) {
      if (strncmp(p->buffer + p->pos, "?>", 2) == 0) {
        p->pos += 2;
        break;
      }
      p->pos++;
    }
  }
}

// Skip whitespace and comments
static void xml_parse_skip_ws_and_comments(text_parse_ctx_t* p) {
  while (true) {
    size_t old_pos = p->pos;
    xml_parse_skip_whitespace(p);
    xml_parse_skip_comments(p);
    xml_parse_skip_declaration(p);
    if (p->pos == old_pos) break;
  }
}

// Parse a tag name (letters, digits, underscore, hyphen)
static const char* xml_parse_tag_name(text_parse_ctx_t* p) {
  size_t start = p->pos;
  while (p->pos < p->size) {
    char c = p->buffer[p->pos];
    if (isalnum((unsigned char)c) || c == '_' || c == '-' || c == ':') {
      p->pos++;
    } else {
      break;
    }
  }
  size_t len = p->pos - start;
  if (len == 0) return NULL;

  if (!text_parse_ensure_temp(p, len)) return NULL;
  memcpy(p->temp_string, p->buffer + start, len);
  p->temp_string[len] = '\0';
  return p->temp_string;
}

// Decode predefined XML entities and decimal/hexadecimal character references.
// Decoding cannot expand beyond the encoded source length.
static const char* xml_decode_entities(text_parse_ctx_t* p, size_t start, size_t len) {
  if (!text_parse_ensure_temp(p, len)) return NULL;
  const char* input = p->buffer + start;
  size_t out = 0;
  for (size_t i = 0; i < len;) {
    if (input[i] != '&') {
      p->temp_string[out++] = input[i++];
      continue;
    }
    uint32_t code = 0;
    size_t end = i;
    while (end < len && input[end] != ';') end++;
    if (end == len) return NULL;
    size_t entity_len = end - i + 1;
    if (entity_len == 4 && memcmp(input + i, "&lt;", 4) == 0) code = '<';
    else if (entity_len == 4 && memcmp(input + i, "&gt;", 4) == 0) code = '>';
    else if (entity_len == 5 && memcmp(input + i, "&amp;", 5) == 0) code = '&';
    else if (entity_len == 6 && memcmp(input + i, "&quot;", 6) == 0) code = '"';
    else if (entity_len == 6 && memcmp(input + i, "&apos;", 6) == 0) code = '\'';
    else if (i + 2 < end && input[i + 1] == '#') {
      size_t digit_pos = i + 2;
      unsigned int base = 10;
      if (input[digit_pos] == 'x') {
        base = 16;
        digit_pos++;
      }
      if (digit_pos == end) return NULL;
      for (; digit_pos < end; digit_pos++) {
        unsigned int digit;
        char ch = input[digit_pos];
        if (ch >= '0' && ch <= '9') digit = (unsigned int)(ch - '0');
        else if (base == 16 && ch >= 'a' && ch <= 'f') digit = (unsigned int)(ch - 'a' + 10);
        else if (base == 16 && ch >= 'A' && ch <= 'F') digit = (unsigned int)(ch - 'A' + 10);
        else return NULL;
        if (digit >= base || code > (0x10ffffu - digit) / base) return NULL;
        code = code * base + digit;
      }
    } else return NULL;
    if (!xml_valid_character(code)) return NULL;
    if (code < 0x80) {
      p->temp_string[out++] = (char)code;
    } else if (code < 0x800) {
      p->temp_string[out++] = (char)(0xc0 | (code >> 6));
      p->temp_string[out++] = (char)(0x80 | (code & 0x3f));
    } else if (code < 0x10000) {
      p->temp_string[out++] = (char)(0xe0 | (code >> 12));
      p->temp_string[out++] = (char)(0x80 | ((code >> 6) & 0x3f));
      p->temp_string[out++] = (char)(0x80 | (code & 0x3f));
    } else {
      p->temp_string[out++] = (char)(0xf0 | (code >> 18));
      p->temp_string[out++] = (char)(0x80 | ((code >> 12) & 0x3f));
      p->temp_string[out++] = (char)(0x80 | ((code >> 6) & 0x3f));
      p->temp_string[out++] = (char)(0x80 | (code & 0x3f));
    }
    i = end + 1;
  }
  p->temp_string[out] = '\0';
  return p->temp_string;
}

// Parse attribute value (after the = sign, handles quotes)
static const char* xml_parse_attribute_value(text_parse_ctx_t* p) {
  xml_parse_skip_whitespace(p);
  if (p->pos >= p->size) return NULL;

  char quote = p->buffer[p->pos];
  if (quote != '"' && quote != '\'') return NULL;
  p->pos++;

  size_t start = p->pos;
  while (p->pos < p->size && p->buffer[p->pos] != quote) {
    p->pos++;
  }

  size_t len = p->pos - start;
  if (p->pos >= p->size) return NULL;
  p->pos++; // Skip closing quote

  return xml_decode_entities(p, start, len);
}

// Parse an XML tag and extract attributes
static bool xml_parse_tag(text_parse_ctx_t* p, xml_tag_info_t* info) {
  memset(info, 0, sizeof(*info));

  xml_parse_skip_ws_and_comments(p);
  if (p->pos >= p->size || p->buffer[p->pos] != '<') return false;
  p->pos++;

  // Check for closing tag
  if (p->pos < p->size && p->buffer[p->pos] == '/') {
    info->is_closing_tag = true;
    p->pos++;
  }

  // Parse tag name
  const char* name = xml_parse_tag_name(p);
  if (!name) return false;
  strncpy(info->tag_name, name, sizeof(info->tag_name) - 1);

  // Parse attributes
  while (true) {
    xml_parse_skip_whitespace(p);
    if (p->pos >= p->size) return false;

    char c = p->buffer[p->pos];
    if (c == '>') {
      p->pos++;
      break;
    }
    if (c == '/') {
      p->pos++;
      xml_parse_skip_whitespace(p);
      if (p->pos < p->size && p->buffer[p->pos] == '>') {
        p->pos++;
        info->is_self_closing = true;
        break;
      }
      return false;
    }

    // Parse attribute name
    const char* attr_name = xml_parse_tag_name(p);
    if (!attr_name) break;

    // Make a copy of the attribute name
    char attr_name_copy[64];
    strncpy(attr_name_copy, attr_name, sizeof(attr_name_copy) - 1);
    attr_name_copy[sizeof(attr_name_copy) - 1] = '\0';

    xml_parse_skip_whitespace(p);
    if (p->pos >= p->size || p->buffer[p->pos] != '=') continue;
    p->pos++;

    size_t attr_value_pos = p->pos;
    const char* attr_value = xml_parse_attribute_value(p);
    if (!attr_value) return false;

    if (strcmp(attr_name_copy, "type") == 0) {
      strncpy(info->type_attr, attr_value, sizeof(info->type_attr) - 1);
    } else if (strcmp(attr_name_copy, "name") == 0) {
      info->has_name_attr = true;
      info->name_attr_pos = attr_value_pos;
    } else if (strcmp(attr_name_copy, "dims") == 0) {
      strncpy(info->dims_attr, attr_value, sizeof(info->dims_attr) - 1);
    }
  }

  return true;
}

// Parse text content until < is found, handling XML entities
static const char* xml_parse_text_content(text_parse_ctx_t* p) {
  size_t start = p->pos;

  // Find the end of content (before next tag)
  while (p->pos < p->size && p->buffer[p->pos] != '<') {
    p->pos++;
  }

  return xml_decode_entities(p, start, p->pos - start);
}

// Skip to after a closing tag
static bool xml_skip_to_close_tag(text_parse_ctx_t* p, const char* tag_name) {
  xml_tag_info_t info;
  while (p->pos < p->size) {
    xml_parse_skip_ws_and_comments(p);
    if (p->pos >= p->size) return false;
    if (p->buffer[p->pos] != '<') {
      p->pos++;
      continue;
    }
    size_t saved = p->pos;
    if (xml_parse_tag(p, &info)) {
      if (info.is_closing_tag && strcmp(info.tag_name, tag_name) == 0) {
        return true;
      }
    } else {
      p->pos = saved + 1;
    }
  }
  return false;
}

// Determine the type from a tag or type attribute
static olib_object_type_t xml_get_type_from_tag(const xml_tag_info_t* info) {
  const char* type_str = info->type_attr[0] ? info->type_attr : info->tag_name;

  if (strcmp(type_str, "int") == 0) return OLIB_OBJECT_TYPE_INT;
  if (strcmp(type_str, "uint") == 0) return OLIB_OBJECT_TYPE_UINT;
  if (strcmp(type_str, "float") == 0) return OLIB_OBJECT_TYPE_FLOAT;
  if (strcmp(type_str, "string") == 0) return OLIB_OBJECT_TYPE_STRING;
  if (strcmp(type_str, "bool") == 0) return OLIB_OBJECT_TYPE_BOOL;
  if (strcmp(type_str, "list") == 0) return OLIB_OBJECT_TYPE_LIST;  // Legacy support
  if (strcmp(type_str, "list") == 0) return OLIB_OBJECT_TYPE_LIST;
  if (strcmp(type_str, "struct") == 0) return OLIB_OBJECT_TYPE_STRUCT;

  return OLIB_OBJECT_TYPE_MAX;
}

// #############################################################################
// Read callbacks
// #############################################################################

static olib_object_type_t xml_read_peek(void* ctx) {
  xml_ctx_t* c = (xml_ctx_t*)ctx;
  text_parse_ctx_t* p = &c->parse;

  // If we have a pending type from a key/item tag, return it
  if (c->has_pending_type) {
    return c->pending_value_type;
  }

  xml_parse_skip_ws_and_comments(p);
  if (p->pos >= p->size) return OLIB_OBJECT_TYPE_MAX;
  if (p->buffer[p->pos] != '<') return OLIB_OBJECT_TYPE_MAX;

  // Save position for peeking
  size_t saved_pos = p->pos;

  xml_tag_info_t info;
  if (!xml_parse_tag(p, &info)) {
    p->pos = saved_pos;
    return OLIB_OBJECT_TYPE_MAX;
  }

  // Restore position
  p->pos = saved_pos;

  // Skip root or closing tags during peek
  if (strcmp(info.tag_name, "olib") == 0 || strcmp(info.tag_name, "root") == 0) {
    // The root element IS a struct
    return OLIB_OBJECT_TYPE_STRUCT;
  }

  if (info.is_closing_tag) {
    return OLIB_OBJECT_TYPE_MAX;
  }

  return xml_get_type_from_tag(&info);
}

static bool xml_numeric_tail_empty(const char* tail) {
  while (*tail == ' ' || *tail == '\t' || *tail == '\n' || *tail == '\r') tail++;
  return *tail == '\0';
}

static bool xml_read_int(void* ctx, int64_t* value) {
  xml_ctx_t* c = (xml_ctx_t*)ctx;
  text_parse_ctx_t* p = &c->parse;

  xml_parse_skip_ws_and_comments(p);

  // If we have a pending type, we're reading from inside a key/item element
  // so we don't need to parse an opening tag
  if (!c->has_pending_type) {
    xml_tag_info_t info;
    if (!xml_parse_tag(p, &info)) return false;
  } else {
    c->has_pending_type = false;
  }

  const char* content = xml_parse_text_content(p);
  if (!content) return false;

  errno = 0;
  char* end;
  long long parsed = strtoll(content, &end, 10);
  if (end == content || errno == ERANGE || parsed < INT64_MIN || parsed > INT64_MAX ||
      !xml_numeric_tail_empty(end)) return false;

  // Skip closing tag
  xml_tag_info_t close_info;
  if (!xml_parse_tag(p, &close_info) || !close_info.is_closing_tag) return false;

  *value = (int64_t)parsed;
  return true;
}

static bool xml_read_uint(void* ctx, uint64_t* value) {
  xml_ctx_t* c = (xml_ctx_t*)ctx;
  text_parse_ctx_t* p = &c->parse;

  xml_parse_skip_ws_and_comments(p);

  if (!c->has_pending_type) {
    xml_tag_info_t info;
    if (!xml_parse_tag(p, &info)) return false;
  } else {
    c->has_pending_type = false;
  }

  const char* content = xml_parse_text_content(p);
  if (!content) return false;

  while (*content == ' ' || *content == '\t' || *content == '\n' || *content == '\r') content++;
  if (*content == '-') return false;
  errno = 0;
  char* end;
  unsigned long long parsed = strtoull(content, &end, 10);
  if (end == content || errno == ERANGE || parsed > UINT64_MAX ||
      !xml_numeric_tail_empty(end)) return false;

  // Skip closing tag
  xml_tag_info_t close_info;
  if (!xml_parse_tag(p, &close_info) || !close_info.is_closing_tag) return false;

  *value = (uint64_t)parsed;
  return true;
}

static bool xml_read_float(void* ctx, double* value) {
  xml_ctx_t* c = (xml_ctx_t*)ctx;
  text_parse_ctx_t* p = &c->parse;

  xml_parse_skip_ws_and_comments(p);

  if (!c->has_pending_type) {
    xml_tag_info_t info;
    if (!xml_parse_tag(p, &info)) return false;
  } else {
    c->has_pending_type = false;
  }

  const char* content = xml_parse_text_content(p);
  if (!content) return false;

  errno = 0;
  char* end;
  double parsed = strtod(content, &end);
  if (end == content || !xml_numeric_tail_empty(end) ||
      (errno == ERANGE && (parsed == 0.0 || isinf(parsed)))) return false;

  // Skip closing tag
  xml_tag_info_t close_info;
  if (!xml_parse_tag(p, &close_info) || !close_info.is_closing_tag) return false;

  *value = parsed;
  return true;
}

static bool xml_read_string(void* ctx, const char** value) {
  xml_ctx_t* c = (xml_ctx_t*)ctx;
  text_parse_ctx_t* p = &c->parse;

  xml_parse_skip_ws_and_comments(p);

  if (!c->has_pending_type) {
    xml_tag_info_t info;
    if (!xml_parse_tag(p, &info)) return false;
  } else {
    c->has_pending_type = false;
  }

  const char* content = xml_parse_text_content(p);
  if (!content) return false;

  // Make a more permanent copy in temp_string
  size_t len = strlen(content);
  if (!text_parse_ensure_temp(p, len + 1)) return false;

  // Content is already in temp_string from xml_parse_text_content
  // so we need to preserve it before parsing the closing tag

  // Allocate a separate buffer for the string
  char* str_copy = olib_malloc(len + 1);
  if (!str_copy) return false;
  strcpy(str_copy, content);

  // Skip closing tag
  xml_tag_info_t close_info;
  xml_parse_tag(p, &close_info);

  // Copy back to temp_string for return
  if (!text_parse_ensure_temp(p, len + 1)) {
    olib_free(str_copy);
    return false;
  }
  strcpy(p->temp_string, str_copy);
  olib_free(str_copy);

  *value = p->temp_string;
  return true;
}

static bool xml_read_bool(void* ctx, bool* value) {
  xml_ctx_t* c = (xml_ctx_t*)ctx;
  text_parse_ctx_t* p = &c->parse;

  xml_parse_skip_ws_and_comments(p);

  if (!c->has_pending_type) {
    xml_tag_info_t info;
    if (!xml_parse_tag(p, &info)) return false;
  } else {
    c->has_pending_type = false;
  }

  const char* content = xml_parse_text_content(p);
  if (!content) return false;

  while (*content == ' ' || *content == '\t' || *content == '\n' || *content == '\r') content++;
  bool parsed;
  if (strncmp(content, "true", 4) == 0 && xml_numeric_tail_empty(content + 4)) parsed = true;
  else if (strncmp(content, "false", 5) == 0 && xml_numeric_tail_empty(content + 5)) parsed = false;
  else if (content[0] == '1' && xml_numeric_tail_empty(content + 1)) parsed = true;
  else if (content[0] == '0' && xml_numeric_tail_empty(content + 1)) parsed = false;
  else return false;

  xml_tag_info_t close_info;
  if (!xml_parse_tag(p, &close_info) || !close_info.is_closing_tag) return false;
  *value = parsed;
  return true;
}

static bool xml_read_list_begin(void* ctx, size_t* size) {
  xml_ctx_t* c = (xml_ctx_t*)ctx;
  text_parse_ctx_t* p = &c->parse;

  xml_parse_skip_ws_and_comments(p);

  if (!c->has_pending_type) {
    xml_tag_info_t info;
    if (!xml_parse_tag(p, &info)) return false;
  } else {
    c->has_pending_type = false;
  }

  // Count child elements
  size_t saved_pos = p->pos;
  size_t count = 0;
  int depth = 1;

  while (p->pos < p->size && depth > 0) {
    xml_parse_skip_ws_and_comments(p);
    if (p->pos >= p->size) break;
    if (p->buffer[p->pos] != '<') {
      p->pos++;
      continue;
    }

    xml_tag_info_t child_info;
    if (xml_parse_tag(p, &child_info)) {
      if (child_info.is_closing_tag) {
        depth--;
      } else if (!child_info.is_self_closing) {
        if (depth == 1) count++;
        depth++;
      } else {
        if (depth == 1) count++;
      }
    }
  }

  p->pos = saved_pos;
  *size = count;
  return true;
}

static bool xml_read_list_end(void* ctx) {
  xml_ctx_t* c = (xml_ctx_t*)ctx;
  text_parse_ctx_t* p = &c->parse;

  xml_parse_skip_ws_and_comments(p);

  xml_tag_info_t info;
  if (!xml_parse_tag(p, &info)) return false;

  return info.is_closing_tag;
}

static bool xml_read_struct_begin(void* ctx) {
  xml_ctx_t* c = (xml_ctx_t*)ctx;
  text_parse_ctx_t* p = &c->parse;

  xml_parse_skip_ws_and_comments(p);

  if (!c->has_pending_type) {
    xml_tag_info_t info;
    if (!xml_parse_tag(p, &info)) return false;
  } else {
    c->has_pending_type = false;
  }

  return true;
}

static bool xml_read_struct_key(void* ctx, const char** key) {
  xml_ctx_t* c = (xml_ctx_t*)ctx;
  text_parse_ctx_t* p = &c->parse;

  xml_parse_skip_ws_and_comments(p);

  // Check if we're at a closing tag
  if (p->pos >= p->size) return false;
  if (p->buffer[p->pos] != '<') return false;

  size_t saved_pos = p->pos;
  xml_tag_info_t info;
  if (!xml_parse_tag(p, &info)) return false;

  if (info.is_closing_tag) {
    // Restore position so struct_end can consume it
    p->pos = saved_pos;
    return false;
  }

  // Restore position - the actual value reading will consume the tag
  p->pos = saved_pos;

  // For struct keys, we need to peek at the tag
  if (!xml_parse_tag(p, &info)) return false;

  // Attribute values can be arbitrarily long. Decode the name again into the
  // parser's temporary buffer rather than truncating it in the tag metadata.
  if (info.has_name_attr) {
    size_t value_pos = p->pos;
    p->pos = info.name_attr_pos;
    *key = xml_parse_attribute_value(p);
    p->pos = value_pos;
    if (!*key) return false;
  } else {
    size_t len = strlen(info.tag_name);
    if (!text_parse_ensure_temp(p, len + 1)) return false;
    strcpy(p->temp_string, info.tag_name);
    *key = p->temp_string;
  }

  // Store the complete tag info and type for value reading
  c->pending_tag_info = info;
  c->pending_value_type = xml_get_type_from_tag(&info);
  c->has_pending_type = true;

  return true;
}

static bool xml_read_struct_end(void* ctx) {
  xml_ctx_t* c = (xml_ctx_t*)ctx;
  text_parse_ctx_t* p = &c->parse;

  xml_parse_skip_ws_and_comments(p);

  xml_tag_info_t info;
  if (!xml_parse_tag(p, &info)) return false;

  return info.is_closing_tag;
}


// #############################################################################
// Lifecycle callbacks
// #############################################################################

static void xml_free_ctx(void* ctx) {
  xml_ctx_t* c = (xml_ctx_t*)ctx;
  if (c->write_buffer) olib_free(c->write_buffer);
  text_parse_free(&c->parse);
  olib_free(c);
}

static bool xml_init_write(void* ctx) {
  xml_ctx_t* c = (xml_ctx_t*)ctx;
  c->write_size = 0;
  c->indent_level = 0;
  c->in_list = false;
  c->list_first_item = true;
  c->in_struct = false;
  c->struct_first_item = true;
  c->pending_key = NULL;
  c->needs_root_close = false;
  c->container_depth = 0;

  // Write XML declaration and root element
  if (!xml_write_str(c, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n")) return false;
  if (!xml_write_str(c, "<olib>\n")) return false;
  c->indent_level = 1;
  c->needs_root_close = true;

  return true;
}

static bool xml_finish_write(void* ctx, uint8_t** out_data, size_t* out_size) {
  xml_ctx_t* c = (xml_ctx_t*)ctx;
  if (!out_data || !out_size) {
    return false;
  }

  // Close root element
  if (c->needs_root_close) {
    if (!xml_write_str(c, "\n</olib>\n")) return false;
  }

  // Add null terminator
  if (!xml_ensure_write_capacity(c, 1)) return false;
  c->write_buffer[c->write_size] = '\0';

  *out_data = (uint8_t*)c->write_buffer;
  *out_size = c->write_size;

  c->write_buffer = NULL;
  c->write_capacity = 0;
  c->write_size = 0;
  c->needs_root_close = false;
  return true;
}

static bool xml_init_read(void* ctx, const uint8_t* data, size_t size) {
  xml_ctx_t* c = (xml_ctx_t*)ctx;
  if (!xml_valid_characters(data, size)) return false;
  text_parse_init(&c->parse, (const char*)data, size);

  // Initialize read state
  c->has_pending_type = false;
  c->pending_value_type = OLIB_OBJECT_TYPE_MAX;

  // Skip XML declaration and comments
  xml_parse_skip_declaration(&c->parse);
  xml_parse_skip_ws_and_comments(&c->parse);

  // Peek at the root element to determine format
  // - <olib> is a wrapper element (serializer's format) - skip it
  // - <root> is the struct container (sample file format) - keep it
  size_t saved_pos = c->parse.pos;
  xml_tag_info_t info;
  if (xml_parse_tag(&c->parse, &info)) {
    if (strcmp(info.tag_name, "olib") == 0) {
      // <olib> is a wrapper, skip it - content follows inside
      // Position is already past <olib>, leave it there
    } else {
      // Not <olib>, restore position so peek/read can see it
      // This handles <root> and other formats where root IS the struct
      c->parse.pos = saved_pos;
    }
  } else {
    c->parse.pos = saved_pos;
  }

  return true;
}

static bool xml_finish_read(void* ctx) {
  xml_ctx_t* c = (xml_ctx_t*)ctx;
  text_parse_reset(&c->parse);
  return true;
}

// #############################################################################
// Public API
// #############################################################################

OLIB_API olib_serializer_t* olib_serializer_new_xml() {
  xml_ctx_t* ctx = olib_calloc(1, sizeof(xml_ctx_t));
  if (!ctx) {
    return NULL;
  }

  olib_serializer_config_t config = {
    .user_data = ctx,
    .text_based = true,
    .free_ctx = xml_free_ctx,
    .init_write = xml_init_write,
    .finish_write = xml_finish_write,
    .init_read = xml_init_read,
    .finish_read = xml_finish_read,

    .write_int = xml_write_int,
    .write_uint = xml_write_uint,
    .write_float = xml_write_float,
    .write_string = xml_write_string,
    .write_bool = xml_write_bool,
    .write_list_begin = xml_write_list_begin,
    .write_list_end = xml_write_list_end,
    .write_struct_begin = xml_write_struct_begin,
    .write_struct_key = xml_write_struct_key,
    .write_struct_end = xml_write_struct_end,

    .read_peek = xml_read_peek,
    .read_int = xml_read_int,
    .read_uint = xml_read_uint,
    .read_float = xml_read_float,
    .read_string = xml_read_string,
    .read_bool = xml_read_bool,
    .read_list_begin = xml_read_list_begin,
    .read_list_end = xml_read_list_end,
    .read_struct_begin = xml_read_struct_begin,
    .read_struct_key = xml_read_struct_key,
    .read_struct_end = xml_read_struct_end,
  };

  olib_serializer_t* serializer = olib_serializer_new(&config);
  if (!serializer) {
    olib_free(ctx);
    return NULL;
  }

  return serializer;
}
