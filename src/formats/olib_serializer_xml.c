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

// #############################################################################
// Context structure for XML serialization
// #############################################################################

typedef struct {
  // Write mode
  char* write_buffer;
  size_t write_capacity;
  size_t write_size;
  int indent_level;
  bool in_array;
  bool array_first_item;
  bool in_struct;
  bool struct_first_item;
  const char* pending_key;
  bool needs_root_close;

  // Read mode (using shared parsing utilities)
  text_parse_ctx_t parse;
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

// Write XML-escaped string content (not the quotes, just the content)
static bool xml_write_escaped(xml_ctx_t* ctx, const char* str) {
  if (!str) return true;
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
  if (!xml_write_escaped(ctx, value)) return false;
  if (!xml_write_close_tag(ctx, tag)) return false;
  return true;
}

// Handle struct key prefix for values
static bool xml_write_struct_value_begin(xml_ctx_t* ctx, const char* type_tag) {
  if (ctx->in_struct && ctx->pending_key) {
    // Inside struct: <key name="field" type="int">value</key>
    if (!xml_write_str(ctx, "<key name=\"")) return false;
    if (!xml_write_escaped(ctx, ctx->pending_key)) return false;
    if (!xml_write_str(ctx, "\" type=\"")) return false;
    if (!xml_write_str(ctx, type_tag)) return false;
    if (!xml_write_str(ctx, "\">")) return false;
    ctx->pending_key = NULL;
    return true;
  } else if (ctx->in_array) {
    // Inside array: <item type="int">value</item>
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
  } else if (ctx->in_array) {
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

  if (c->in_array) {
    if (!c->array_first_item) {
      if (!xml_write_char(c, '\n')) return false;
    }
    c->array_first_item = false;
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

  if (c->in_array) {
    if (!c->array_first_item) {
      if (!xml_write_char(c, '\n')) return false;
    }
    c->array_first_item = false;
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

  if (c->in_array) {
    if (!c->array_first_item) {
      if (!xml_write_char(c, '\n')) return false;
    }
    c->array_first_item = false;
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
  snprintf(buf, sizeof(buf), "%g", value);
  if (!xml_write_str(c, buf)) return false;

  if (!xml_write_struct_value_end(c, "float")) return false;
  return true;
}

static bool xml_write_string(void* ctx, const char* value) {
  xml_ctx_t* c = (xml_ctx_t*)ctx;

  if (c->in_array) {
    if (!c->array_first_item) {
      if (!xml_write_char(c, '\n')) return false;
    }
    c->array_first_item = false;
    if (!xml_write_indent(c)) return false;
  } else if (c->in_struct) {
    if (!c->struct_first_item) {
      if (!xml_write_char(c, '\n')) return false;
    }
    c->struct_first_item = false;
    if (!xml_write_indent(c)) return false;
  }

  if (!xml_write_struct_value_begin(c, "string")) return false;
  if (!xml_write_escaped(c, value)) return false;
  if (!xml_write_struct_value_end(c, "string")) return false;
  return true;
}

static bool xml_write_bool(void* ctx, bool value) {
  xml_ctx_t* c = (xml_ctx_t*)ctx;

  if (c->in_array) {
    if (!c->array_first_item) {
      if (!xml_write_char(c, '\n')) return false;
    }
    c->array_first_item = false;
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

static bool xml_write_array_begin(void* ctx, size_t size) {
  (void)size;
  xml_ctx_t* c = (xml_ctx_t*)ctx;

  if (c->in_array) {
    if (!c->array_first_item) {
      if (!xml_write_char(c, '\n')) return false;
    }
    c->array_first_item = false;
    if (!xml_write_indent(c)) return false;
  } else if (c->in_struct) {
    if (!c->struct_first_item) {
      if (!xml_write_char(c, '\n')) return false;
    }
    c->struct_first_item = false;
    if (!xml_write_indent(c)) return false;
  }

  // Handle struct key for nested containers
  if (c->in_struct && c->pending_key) {
    if (!xml_write_str(c, "<key name=\"")) return false;
    if (!xml_write_escaped(c, c->pending_key)) return false;
    if (!xml_write_str(c, "\" type=\"array\">")) return false;
    c->pending_key = NULL;
  } else if (c->in_array) {
    if (!xml_write_str(c, "<item type=\"array\">")) return false;
  } else {
    if (!xml_write_str(c, "<array>")) return false;
  }

  if (!xml_write_char(c, '\n')) return false;
  c->indent_level++;
  c->in_array = true;
  c->array_first_item = true;
  return true;
}

static bool xml_write_array_end(void* ctx) {
  xml_ctx_t* c = (xml_ctx_t*)ctx;
  c->indent_level--;
  if (!xml_write_char(c, '\n')) return false;
  if (!xml_write_indent(c)) return false;

  // Determine what closing tag to use based on context
  // Since we're ending an array, we were in_array before
  c->in_array = false;

  // For simplicity, always use </array> since the outer context
  // determines the opening tag type (key or item)
  if (!xml_write_str(c, "</array>")) return false;

  return true;
}

static bool xml_write_struct_begin(void* ctx) {
  xml_ctx_t* c = (xml_ctx_t*)ctx;

  if (c->in_array) {
    if (!c->array_first_item) {
      if (!xml_write_char(c, '\n')) return false;
    }
    c->array_first_item = false;
    if (!xml_write_indent(c)) return false;
  } else if (c->in_struct) {
    if (!c->struct_first_item) {
      if (!xml_write_char(c, '\n')) return false;
    }
    c->struct_first_item = false;
    if (!xml_write_indent(c)) return false;
  }

  // Handle struct key for nested containers
  if (c->in_struct && c->pending_key) {
    if (!xml_write_str(c, "<key name=\"")) return false;
    if (!xml_write_escaped(c, c->pending_key)) return false;
    if (!xml_write_str(c, "\" type=\"struct\">")) return false;
    c->pending_key = NULL;
  } else if (c->in_array) {
    if (!xml_write_str(c, "<item type=\"struct\">")) return false;
  } else {
    if (!xml_write_str(c, "<struct>")) return false;
  }

  if (!xml_write_char(c, '\n')) return false;
  c->indent_level++;
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
  if (!xml_write_str(c, "</struct>")) return false;
  c->in_struct = (c->indent_level > 0);
  return true;
}

static bool xml_write_matrix(void* ctx, size_t ndims, const size_t* dims, const double* data) {
  xml_ctx_t* c = (xml_ctx_t*)ctx;

  if (c->in_array) {
    if (!c->array_first_item) {
      if (!xml_write_char(c, '\n')) return false;
    }
    c->array_first_item = false;
    if (!xml_write_indent(c)) return false;
  } else if (c->in_struct) {
    if (!c->struct_first_item) {
      if (!xml_write_char(c, '\n')) return false;
    }
    c->struct_first_item = false;
    if (!xml_write_indent(c)) return false;
  }

  // Build dims string like "2,3,4"
  char dims_str[256] = "";
  size_t dims_len = 0;
  for (size_t i = 0; i < ndims; i++) {
    if (i > 0) {
      dims_str[dims_len++] = ',';
    }
    dims_len += snprintf(dims_str + dims_len, sizeof(dims_str) - dims_len, "%zu", dims[i]);
  }

  // Handle struct key for matrix
  if (c->in_struct && c->pending_key) {
    if (!xml_write_str(c, "<key name=\"")) return false;
    if (!xml_write_escaped(c, c->pending_key)) return false;
    if (!xml_write_str(c, "\" type=\"matrix\" dims=\"")) return false;
    if (!xml_write_str(c, dims_str)) return false;
    if (!xml_write_str(c, "\">")) return false;
    c->pending_key = NULL;
  } else if (c->in_array) {
    if (!xml_write_str(c, "<item type=\"matrix\" dims=\"")) return false;
    if (!xml_write_str(c, dims_str)) return false;
    if (!xml_write_str(c, "\">")) return false;
  } else {
    if (!xml_write_str(c, "<matrix dims=\"")) return false;
    if (!xml_write_str(c, dims_str)) return false;
    if (!xml_write_str(c, "\">")) return false;
  }

  // Write data as space-separated values
  size_t total = 1;
  for (size_t i = 0; i < ndims; i++) {
    total *= dims[i];
  }

  for (size_t i = 0; i < total; i++) {
    if (i > 0) {
      if (!xml_write_char(c, ' ')) return false;
    }
    char buf[64];
    snprintf(buf, sizeof(buf), "%g", data[i]);
    if (!xml_write_str(c, buf)) return false;
  }

  // Close tag
  if (c->in_struct) {
    if (!xml_write_str(c, "</key>")) return false;
  } else if (c->in_array) {
    if (!xml_write_str(c, "</item>")) return false;
  } else {
    if (!xml_write_str(c, "</matrix>")) return false;
  }

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

  // Unescape XML entities
  if (!text_parse_ensure_temp(p, len + 1)) return NULL;

  size_t out = 0;
  for (size_t i = start; i < start + len; i++) {
    if (p->buffer[i] == '&') {
      if (i + 4 <= start + len && strncmp(p->buffer + i, "&lt;", 4) == 0) {
        p->temp_string[out++] = '<';
        i += 3;
      } else if (i + 4 <= start + len && strncmp(p->buffer + i, "&gt;", 4) == 0) {
        p->temp_string[out++] = '>';
        i += 3;
      } else if (i + 5 <= start + len && strncmp(p->buffer + i, "&amp;", 5) == 0) {
        p->temp_string[out++] = '&';
        i += 4;
      } else if (i + 6 <= start + len && strncmp(p->buffer + i, "&quot;", 6) == 0) {
        p->temp_string[out++] = '"';
        i += 5;
      } else if (i + 6 <= start + len && strncmp(p->buffer + i, "&apos;", 6) == 0) {
        p->temp_string[out++] = '\'';
        i += 5;
      } else {
        p->temp_string[out++] = p->buffer[i];
      }
    } else {
      p->temp_string[out++] = p->buffer[i];
    }
  }
  p->temp_string[out] = '\0';
  return p->temp_string;
}

// Structure to hold parsed tag info
typedef struct {
  char tag_name[64];
  char type_attr[32];
  char name_attr[128];
  char dims_attr[128];
  bool is_self_closing;
  bool is_closing_tag;
} xml_tag_info_t;

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

    const char* attr_value = xml_parse_attribute_value(p);
    if (!attr_value) continue;

    if (strcmp(attr_name_copy, "type") == 0) {
      strncpy(info->type_attr, attr_value, sizeof(info->type_attr) - 1);
    } else if (strcmp(attr_name_copy, "name") == 0) {
      strncpy(info->name_attr, attr_value, sizeof(info->name_attr) - 1);
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

  size_t len = p->pos - start;
  if (!text_parse_ensure_temp(p, len + 1)) return NULL;

  // Unescape XML entities
  size_t out = 0;
  for (size_t i = start; i < start + len; i++) {
    if (p->buffer[i] == '&') {
      if (i + 4 <= start + len && strncmp(p->buffer + i, "&lt;", 4) == 0) {
        p->temp_string[out++] = '<';
        i += 3;
      } else if (i + 4 <= start + len && strncmp(p->buffer + i, "&gt;", 4) == 0) {
        p->temp_string[out++] = '>';
        i += 3;
      } else if (i + 5 <= start + len && strncmp(p->buffer + i, "&amp;", 5) == 0) {
        p->temp_string[out++] = '&';
        i += 4;
      } else if (i + 6 <= start + len && strncmp(p->buffer + i, "&quot;", 6) == 0) {
        p->temp_string[out++] = '"';
        i += 5;
      } else if (i + 6 <= start + len && strncmp(p->buffer + i, "&apos;", 6) == 0) {
        p->temp_string[out++] = '\'';
        i += 5;
      } else {
        p->temp_string[out++] = p->buffer[i];
      }
    } else {
      p->temp_string[out++] = p->buffer[i];
    }
  }
  p->temp_string[out] = '\0';
  return p->temp_string;
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
  if (strcmp(type_str, "array") == 0) return OLIB_OBJECT_TYPE_ARRAY;
  if (strcmp(type_str, "struct") == 0) return OLIB_OBJECT_TYPE_STRUCT;
  if (strcmp(type_str, "matrix") == 0) return OLIB_OBJECT_TYPE_MATRIX;

  return OLIB_OBJECT_TYPE_MAX;
}

// #############################################################################
// Read callbacks
// #############################################################################

static olib_object_type_t xml_read_peek(void* ctx) {
  xml_ctx_t* c = (xml_ctx_t*)ctx;
  text_parse_ctx_t* p = &c->parse;

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
    // Actually consume the root tag
    xml_parse_tag(p, &info);
    return xml_read_peek(ctx);
  }

  if (info.is_closing_tag) {
    return OLIB_OBJECT_TYPE_MAX;
  }

  return xml_get_type_from_tag(&info);
}

static bool xml_read_int(void* ctx, int64_t* value) {
  xml_ctx_t* c = (xml_ctx_t*)ctx;
  text_parse_ctx_t* p = &c->parse;

  xml_parse_skip_ws_and_comments(p);

  xml_tag_info_t info;
  if (!xml_parse_tag(p, &info)) return false;

  const char* content = xml_parse_text_content(p);
  if (!content) return false;

  // Make a copy before parsing closing tag
  char content_copy[128];
  strncpy(content_copy, content, sizeof(content_copy) - 1);
  content_copy[sizeof(content_copy) - 1] = '\0';

  // Skip closing tag
  xml_tag_info_t close_info;
  xml_parse_tag(p, &close_info);

  *value = strtoll(content_copy, NULL, 10);
  return true;
}

static bool xml_read_uint(void* ctx, uint64_t* value) {
  xml_ctx_t* c = (xml_ctx_t*)ctx;
  text_parse_ctx_t* p = &c->parse;

  xml_parse_skip_ws_and_comments(p);

  xml_tag_info_t info;
  if (!xml_parse_tag(p, &info)) return false;

  const char* content = xml_parse_text_content(p);
  if (!content) return false;

  char content_copy[128];
  strncpy(content_copy, content, sizeof(content_copy) - 1);
  content_copy[sizeof(content_copy) - 1] = '\0';

  // Skip closing tag
  xml_tag_info_t close_info;
  xml_parse_tag(p, &close_info);

  *value = strtoull(content_copy, NULL, 10);
  return true;
}

static bool xml_read_float(void* ctx, double* value) {
  xml_ctx_t* c = (xml_ctx_t*)ctx;
  text_parse_ctx_t* p = &c->parse;

  xml_parse_skip_ws_and_comments(p);

  xml_tag_info_t info;
  if (!xml_parse_tag(p, &info)) return false;

  const char* content = xml_parse_text_content(p);
  if (!content) return false;

  char content_copy[128];
  strncpy(content_copy, content, sizeof(content_copy) - 1);
  content_copy[sizeof(content_copy) - 1] = '\0';

  // Skip closing tag
  xml_tag_info_t close_info;
  xml_parse_tag(p, &close_info);

  *value = strtod(content_copy, NULL);
  return true;
}

static bool xml_read_string(void* ctx, const char** value) {
  xml_ctx_t* c = (xml_ctx_t*)ctx;
  text_parse_ctx_t* p = &c->parse;

  xml_parse_skip_ws_and_comments(p);

  xml_tag_info_t info;
  if (!xml_parse_tag(p, &info)) return false;

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

  xml_tag_info_t info;
  if (!xml_parse_tag(p, &info)) return false;

  const char* content = xml_parse_text_content(p);
  if (!content) return false;

  char content_copy[32];
  strncpy(content_copy, content, sizeof(content_copy) - 1);
  content_copy[sizeof(content_copy) - 1] = '\0';

  // Skip closing tag
  xml_tag_info_t close_info;
  xml_parse_tag(p, &close_info);

  // Trim whitespace
  char* s = content_copy;
  while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
  char* e = s + strlen(s) - 1;
  while (e > s && (*e == ' ' || *e == '\t' || *e == '\n' || *e == '\r')) e--;
  *(e + 1) = '\0';

  if (strcmp(s, "true") == 0 || strcmp(s, "1") == 0) {
    *value = true;
  } else {
    *value = false;
  }
  return true;
}

static bool xml_read_array_begin(void* ctx, size_t* size) {
  xml_ctx_t* c = (xml_ctx_t*)ctx;
  text_parse_ctx_t* p = &c->parse;

  xml_parse_skip_ws_and_comments(p);

  xml_tag_info_t info;
  if (!xml_parse_tag(p, &info)) return false;

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

static bool xml_read_array_end(void* ctx) {
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

  xml_tag_info_t info;
  if (!xml_parse_tag(p, &info)) return false;

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

  // For struct keys, we need to peek at the <key name="..."> tag
  if (!xml_parse_tag(p, &info)) return false;

  if (info.name_attr[0]) {
    // Make a copy of the name
    size_t len = strlen(info.name_attr);
    if (!text_parse_ensure_temp(p, len + 1)) return false;
    strcpy(p->temp_string, info.name_attr);
    *key = p->temp_string;

    // Now we need to parse the actual value inside the key element
    // The type attribute tells us what to expect
    // We need to restore state so the read_* functions work correctly

    return true;
  }

  return false;
}

static bool xml_read_struct_end(void* ctx) {
  xml_ctx_t* c = (xml_ctx_t*)ctx;
  text_parse_ctx_t* p = &c->parse;

  xml_parse_skip_ws_and_comments(p);

  xml_tag_info_t info;
  if (!xml_parse_tag(p, &info)) return false;

  return info.is_closing_tag;
}

static bool xml_read_matrix(void* ctx, size_t* ndims, size_t** dims, double** data) {
  xml_ctx_t* c = (xml_ctx_t*)ctx;
  text_parse_ctx_t* p = &c->parse;

  xml_parse_skip_ws_and_comments(p);

  xml_tag_info_t info;
  if (!xml_parse_tag(p, &info)) return false;

  // Parse dims attribute
  size_t dim_count = 0;
  size_t dim_cap = 4;
  size_t* d = olib_malloc(dim_cap * sizeof(size_t));
  if (!d) return false;

  if (info.dims_attr[0]) {
    char* dims_str = info.dims_attr;
    char* token = strtok(dims_str, ",");
    while (token) {
      if (dim_count >= dim_cap) {
        dim_cap *= 2;
        size_t* new_d = olib_realloc(d, dim_cap * sizeof(size_t));
        if (!new_d) {
          olib_free(d);
          return false;
        }
        d = new_d;
      }
      d[dim_count++] = (size_t)atoll(token);
      token = strtok(NULL, ",");
    }
  }

  if (dim_count == 0) {
    olib_free(d);
    return false;
  }

  // Calculate total size
  size_t total = 1;
  for (size_t i = 0; i < dim_count; i++) {
    total *= d[i];
  }

  // Parse content (space-separated values)
  const char* content = xml_parse_text_content(p);
  if (!content) {
    olib_free(d);
    return false;
  }

  // Make a copy for strtok
  size_t content_len = strlen(content);
  char* content_copy = olib_malloc(content_len + 1);
  if (!content_copy) {
    olib_free(d);
    return false;
  }
  strcpy(content_copy, content);

  double* values = olib_malloc(total * sizeof(double));
  if (!values) {
    olib_free(d);
    olib_free(content_copy);
    return false;
  }

  // Parse values
  size_t val_count = 0;
  char* val_token = strtok(content_copy, " \t\n\r");
  while (val_token && val_count < total) {
    values[val_count++] = strtod(val_token, NULL);
    val_token = strtok(NULL, " \t\n\r");
  }

  olib_free(content_copy);

  // Skip closing tag
  xml_tag_info_t close_info;
  xml_parse_tag(p, &close_info);

  *ndims = dim_count;
  *dims = d;
  *data = values;
  return true;
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
  c->in_array = false;
  c->array_first_item = true;
  c->in_struct = false;
  c->struct_first_item = true;
  c->pending_key = NULL;
  c->needs_root_close = false;

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
  text_parse_init(&c->parse, (const char*)data, size);

  // Skip XML declaration and comments
  xml_parse_skip_declaration(&c->parse);
  xml_parse_skip_ws_and_comments(&c->parse);

  // Skip root element if present (olib or root)
  size_t saved_pos = c->parse.pos;
  xml_tag_info_t info;
  if (xml_parse_tag(&c->parse, &info)) {
    if (strcmp(info.tag_name, "olib") != 0 && strcmp(info.tag_name, "root") != 0) {
      // Not a root element, restore position
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
    .write_array_begin = xml_write_array_begin,
    .write_array_end = xml_write_array_end,
    .write_struct_begin = xml_write_struct_begin,
    .write_struct_key = xml_write_struct_key,
    .write_struct_end = xml_write_struct_end,
    .write_matrix = xml_write_matrix,

    .read_peek = xml_read_peek,
    .read_int = xml_read_int,
    .read_uint = xml_read_uint,
    .read_float = xml_read_float,
    .read_string = xml_read_string,
    .read_bool = xml_read_bool,
    .read_array_begin = xml_read_array_begin,
    .read_array_end = xml_read_array_end,
    .read_struct_begin = xml_read_struct_begin,
    .read_struct_key = xml_read_struct_key,
    .read_struct_end = xml_read_struct_end,
    .read_matrix = xml_read_matrix,
  };

  olib_serializer_t* serializer = olib_serializer_new(&config);
  if (!serializer) {
    olib_free(ctx);
    return NULL;
  }

  return serializer;
}
