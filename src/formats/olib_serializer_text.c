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
// Context structure for text serialization
// #############################################################################

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

  // Read mode (using shared parsing utilities)
  text_parse_ctx_t parse;
} text_ctx_t;

// #############################################################################
// Write helpers
// #############################################################################

static bool text_ensure_write_capacity(text_ctx_t* ctx, size_t needed) {
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

static bool text_write_str(text_ctx_t* ctx, const char* str) {
  size_t len = strlen(str);
  if (!text_ensure_write_capacity(ctx, len)) return false;
  memcpy(ctx->write_buffer + ctx->write_size, str, len);
  ctx->write_size += len;
  return true;
}

static bool text_write_char(text_ctx_t* ctx, char c) {
  if (!text_ensure_write_capacity(ctx, 1)) return false;
  ctx->write_buffer[ctx->write_size++] = c;
  return true;
}

static bool text_write_indent(text_ctx_t* ctx) {
  for (int i = 0; i < ctx->indent_level; i++) {
    if (!text_write_char(ctx, '\t')) return false;
  }
  return true;
}

static bool text_write_key_prefix(text_ctx_t* ctx) {
  if (ctx->pending_key) {
    if (ctx->in_struct) {
      // Inside struct: "key: "
      if (!text_write_str(ctx, ctx->pending_key)) return false;
      if (!text_write_str(ctx, ": ")) return false;
    } else {
      // Top-level: "key "
      if (!text_write_str(ctx, ctx->pending_key)) return false;
      if (!text_write_char(ctx, ' ')) return false;
    }
    ctx->pending_key = NULL;
  }
  return true;
}

// #############################################################################
// Write callbacks
// #############################################################################

static bool text_write_int(void* ctx, int64_t value) {
  text_ctx_t* c = (text_ctx_t*)ctx;

  if (c->in_list) {
    if (!c->list_first_item) {
      if (!text_write_str(c, ", ")) return false;
    }
    c->list_first_item = false;
  } else if (c->in_struct) {
    if (!c->struct_first_item) {
      if (!text_write_char(c, '\n')) return false;
    }
    c->struct_first_item = false;
    if (!text_write_indent(c)) return false;
  }

  if (!text_write_key_prefix(c)) return false;

  char buf[32];
  snprintf(buf, sizeof(buf), "%lld", (long long)value);
  return text_write_str(c, buf);
}

static bool text_write_uint(void* ctx, uint64_t value) {
  text_ctx_t* c = (text_ctx_t*)ctx;

  if (c->in_list) {
    if (!c->list_first_item) {
      if (!text_write_str(c, ", ")) return false;
    }
    c->list_first_item = false;
  } else if (c->in_struct) {
    if (!c->struct_first_item) {
      if (!text_write_char(c, '\n')) return false;
    }
    c->struct_first_item = false;
    if (!text_write_indent(c)) return false;
  }

  if (!text_write_key_prefix(c)) return false;

  char buf[32];
  snprintf(buf, sizeof(buf), "%llu", (unsigned long long)value);
  return text_write_str(c, buf);
}

static bool text_write_float(void* ctx, double value) {
  text_ctx_t* c = (text_ctx_t*)ctx;

  if (c->in_list) {
    if (!c->list_first_item) {
      if (!text_write_str(c, ", ")) return false;
    }
    c->list_first_item = false;
  } else if (c->in_struct) {
    if (!c->struct_first_item) {
      if (!text_write_char(c, '\n')) return false;
    }
    c->struct_first_item = false;
    if (!text_write_indent(c)) return false;
  }

  if (!text_write_key_prefix(c)) return false;

  char buf[64];
  snprintf(buf, sizeof(buf), "%g", value);
  return text_write_str(c, buf);
}

static bool text_write_string(void* ctx, const char* value) {
  text_ctx_t* c = (text_ctx_t*)ctx;

  if (c->in_list) {
    if (!c->list_first_item) {
      if (!text_write_str(c, ", ")) return false;
    }
    c->list_first_item = false;
  } else if (c->in_struct) {
    if (!c->struct_first_item) {
      if (!text_write_char(c, '\n')) return false;
    }
    c->struct_first_item = false;
    if (!text_write_indent(c)) return false;
  }

  if (!text_write_key_prefix(c)) return false;

  if (!text_write_char(c, '"')) return false;
  if (value) {
    // Escape special characters
    for (const char* p = value; *p; p++) {
      switch (*p) {
        case '"':
          if (!text_write_str(c, "\\\"")) return false;
          break;
        case '\\':
          if (!text_write_str(c, "\\\\")) return false;
          break;
        case '\n':
          if (!text_write_str(c, "\\n")) return false;
          break;
        case '\r':
          if (!text_write_str(c, "\\r")) return false;
          break;
        case '\t':
          if (!text_write_str(c, "\\t")) return false;
          break;
        default:
          if (!text_write_char(c, *p)) return false;
          break;
      }
    }
  }
  if (!text_write_char(c, '"')) return false;
  return true;
}

static bool text_write_bool(void* ctx, bool value) {
  text_ctx_t* c = (text_ctx_t*)ctx;

  if (c->in_list) {
    if (!c->list_first_item) {
      if (!text_write_str(c, ", ")) return false;
    }
    c->list_first_item = false;
  } else if (c->in_struct) {
    if (!c->struct_first_item) {
      if (!text_write_char(c, '\n')) return false;
    }
    c->struct_first_item = false;
    if (!text_write_indent(c)) return false;
  }

  if (!text_write_key_prefix(c)) return false;

  return text_write_str(c, value ? "true" : "false");
}

static bool text_write_list_begin(void* ctx, size_t size) {
  (void)size;
  text_ctx_t* c = (text_ctx_t*)ctx;

  if (c->in_list) {
    if (!c->list_first_item) {
      if (!text_write_str(c, ", ")) return false;
    }
    c->list_first_item = false;
  } else if (c->in_struct) {
    if (!c->struct_first_item) {
      if (!text_write_char(c, '\n')) return false;
    }
    c->struct_first_item = false;
    if (!text_write_indent(c)) return false;
  }

  if (!text_write_key_prefix(c)) return false;

  if (!text_write_str(c, "[ ")) return false;
  c->in_list = true;
  c->list_first_item = true;
  return true;
}

static bool text_write_list_end(void* ctx) {
  text_ctx_t* c = (text_ctx_t*)ctx;
  if (!text_write_str(c, " ]")) return false;
  c->in_list = false;
  return true;
}

static bool text_write_struct_begin(void* ctx) {
  text_ctx_t* c = (text_ctx_t*)ctx;

  if (c->in_list) {
    if (!c->list_first_item) {
      if (!text_write_str(c, ", ")) return false;
    }
    c->list_first_item = false;
  } else if (c->in_struct) {
    if (!c->struct_first_item) {
      if (!text_write_char(c, '\n')) return false;
    }
    c->struct_first_item = false;
    if (!text_write_indent(c)) return false;
  }

  if (!text_write_key_prefix(c)) return false;

  if (!text_write_str(c, "{\n")) return false;
  c->indent_level++;
  c->in_struct = true;
  c->struct_first_item = true;
  return true;
}

static bool text_write_struct_key(void* ctx, const char* key) {
  text_ctx_t* c = (text_ctx_t*)ctx;
  c->pending_key = key;
  return true;
}

static bool text_write_struct_end(void* ctx) {
  text_ctx_t* c = (text_ctx_t*)ctx;
  c->indent_level--;
  if (!text_write_char(c, '\n')) return false;
  if (!text_write_indent(c)) return false;
  if (!text_write_char(c, '}')) return false;
  c->in_struct = (c->indent_level > 0);
  return true;
}

// #############################################################################
// Read callbacks
// #############################################################################

static olib_object_type_t text_read_peek(void* ctx) {
  text_ctx_t* c = (text_ctx_t*)ctx;
  text_parse_ctx_t* p = &c->parse;
  text_parse_skip_whitespace_and_comments(p);

  // Skip comma if present (between list/struct elements)
  if (p->pos < p->size && p->buffer[p->pos] == ',') {
    p->pos++;
    text_parse_skip_whitespace_and_comments(p);
  }

  if (text_parse_eof(p)) {
    return OLIB_OBJECT_TYPE_MAX;
  }

  char ch = text_parse_peek_raw(p);

  if (ch == '"') {
    return OLIB_OBJECT_TYPE_STRING;
  }
  if (ch == '{') {
    return OLIB_OBJECT_TYPE_STRUCT;
  }
  if (ch == '[') {
    return OLIB_OBJECT_TYPE_LIST;
  }
  if (ch == '-' || ch == '+' || isdigit((unsigned char)ch)) {
    // Peek ahead to determine if int or float
    size_t pos = p->pos;
    if (ch == '-' || ch == '+') pos++;
    while (pos < p->size && isdigit((unsigned char)p->buffer[pos])) {
      pos++;
    }
    if (pos < p->size && (p->buffer[pos] == '.' || p->buffer[pos] == 'e' || p->buffer[pos] == 'E')) {
      return OLIB_OBJECT_TYPE_FLOAT;
    }
    return OLIB_OBJECT_TYPE_INT;
  }
  if (ch == 't' || ch == 'f') {
    if (p->pos + 4 <= p->size && strncmp(p->buffer + p->pos, "true", 4) == 0) {
      return OLIB_OBJECT_TYPE_BOOL;
    }
    if (p->pos + 5 <= p->size && strncmp(p->buffer + p->pos, "false", 5) == 0) {
      return OLIB_OBJECT_TYPE_BOOL;
    }
  }

  return OLIB_OBJECT_TYPE_MAX;
}

static bool text_read_int(void* ctx, int64_t* value) {
  text_ctx_t* c = (text_ctx_t*)ctx;
  text_parse_number_result_t result;
  if (!text_parse_number(&c->parse, &result)) return false;
  *value = result.int_value;
  return true;
}

static bool text_read_uint(void* ctx, uint64_t* value) {
  text_ctx_t* c = (text_ctx_t*)ctx;
  text_parse_number_result_t result;
  if (!text_parse_number(&c->parse, &result)) return false;
  *value = result.uint_value;
  return true;
}

static bool text_read_float(void* ctx, double* value) {
  text_ctx_t* c = (text_ctx_t*)ctx;
  text_parse_number_result_t result;
  if (!text_parse_number(&c->parse, &result)) return false;
  *value = result.is_float ? result.float_value : (double)result.int_value;
  return true;
}

static bool text_read_string(void* ctx, const char** value) {
  text_ctx_t* c = (text_ctx_t*)ctx;
  const char* str = text_parse_quoted_string(&c->parse);
  if (!str) return false;
  *value = str;
  return true;
}

static bool text_read_bool(void* ctx, bool* value) {
  text_ctx_t* c = (text_ctx_t*)ctx;
  text_parse_ctx_t* p = &c->parse;
  text_parse_skip_whitespace(p);

  if (text_parse_match_str(p, "true")) {
    *value = true;
    return true;
  }
  if (text_parse_match_str(p, "false")) {
    *value = false;
    return true;
  }
  return false;
}

static bool text_read_list_begin(void* ctx, size_t* size) {
  text_ctx_t* c = (text_ctx_t*)ctx;
  text_parse_ctx_t* p = &c->parse;

  if (!text_parse_match(p, '[')) return false;

  // Count elements by scanning ahead (without consuming)
  // Elements can be separated by commas, newlines, or both
  size_t pos = p->pos;
  size_t count = 0;
  int depth = 1;
  bool has_content = false;
  bool just_had_separator = true;  // Start true to count first element

  while (pos < p->size && depth > 0) {
    char ch = p->buffer[pos];
    if (ch == '[' || ch == '{') {
      depth++;
      if (depth == 2 && just_had_separator) {
        count++;
        just_had_separator = false;
      }
      has_content = true;
    } else if (ch == ']' || ch == '}') {
      depth--;
    } else if (depth == 1 && (ch == ',' || ch == '\n')) {
      // Comma or newline can be a separator
      if (!just_had_separator && has_content) {
        // This marks the end of a previous element
      }
      just_had_separator = true;
    } else if (ch != ' ' && ch != '\t' && ch != '\r' && depth == 1) {
      if (just_had_separator) {
        count++;
        just_had_separator = false;
      }
      has_content = true;
    }
    pos++;
  }

  *size = count;
  return true;
}

static bool text_read_list_end(void* ctx) {
  text_ctx_t* c = (text_ctx_t*)ctx;
  text_parse_ctx_t* p = &c->parse;
  text_parse_skip_whitespace(p);
  text_parse_match(p, ',');
  return text_parse_match(p, ']');
}

static bool text_read_struct_begin(void* ctx) {
  text_ctx_t* c = (text_ctx_t*)ctx;
  return text_parse_match(&c->parse, '{');
}

static bool text_read_struct_key(void* ctx, const char** key) {
  text_ctx_t* c = (text_ctx_t*)ctx;
  text_parse_ctx_t* p = &c->parse;
  text_parse_skip_whitespace_and_comments(p);

  // Check if we've reached the end of the struct
  if (text_parse_peek_raw(p) == '}') {
    return false;
  }

  // Read identifier
  const char* id = text_parse_identifier(p);
  if (!id) return false;

  // Skip colon
  text_parse_skip_whitespace(p);
  text_parse_match(p, ':');

  *key = id;
  return true;
}

static bool text_read_struct_end(void* ctx) {
  text_ctx_t* c = (text_ctx_t*)ctx;
  return text_parse_match(&c->parse, '}');
}

// #############################################################################
// Lifecycle callbacks
// #############################################################################

static void text_free_ctx(void* ctx) {
  text_ctx_t* c = (text_ctx_t*)ctx;
  if (c->write_buffer) olib_free(c->write_buffer);
  text_parse_free(&c->parse);
  olib_free(c);
}

static bool text_init_write(void* ctx) {
  text_ctx_t* c = (text_ctx_t*)ctx;
  c->write_size = 0;
  c->indent_level = 0;
  c->in_list = false;
  c->list_first_item = true;
  c->in_struct = false;
  c->struct_first_item = true;
  c->pending_key = NULL;
  return true;
}

static bool text_finish_write(void* ctx, uint8_t** out_data, size_t* out_size) {
  text_ctx_t* c = (text_ctx_t*)ctx;
  if (!out_data || !out_size) {
    return false;
  }

  // Add null terminator
  if (!text_ensure_write_capacity(c, 1)) return false;
  c->write_buffer[c->write_size] = '\0';

  *out_data = (uint8_t*)c->write_buffer;
  *out_size = c->write_size;

  c->write_buffer = NULL;
  c->write_capacity = 0;
  c->write_size = 0;
  return true;
}

static bool text_init_read(void* ctx, const uint8_t* data, size_t size) {
  text_ctx_t* c = (text_ctx_t*)ctx;
  text_parse_init(&c->parse, (const char*)data, size);
  return true;
}

static bool text_finish_read(void* ctx) {
  text_ctx_t* c = (text_ctx_t*)ctx;
  text_parse_reset(&c->parse);
  return true;
}

// #############################################################################
// Public API
// #############################################################################

OLIB_API olib_serializer_t* olib_serializer_new_txt() {
  text_ctx_t* ctx = olib_calloc(1, sizeof(text_ctx_t));
  if (!ctx) {
    return NULL;
  }

  olib_serializer_config_t config = {
    .user_data = ctx,
    .text_based = true,
    .free_ctx = text_free_ctx,
    .init_write = text_init_write,
    .finish_write = text_finish_write,
    .init_read = text_init_read,
    .finish_read = text_finish_read,

    .write_int = text_write_int,
    .write_uint = text_write_uint,
    .write_float = text_write_float,
    .write_string = text_write_string,
    .write_bool = text_write_bool,
    .write_list_begin = text_write_list_begin,
    .write_list_end = text_write_list_end,
    .write_struct_begin = text_write_struct_begin,
    .write_struct_key = text_write_struct_key,
    .write_struct_end = text_write_struct_end,

    .read_peek = text_read_peek,
    .read_int = text_read_int,
    .read_uint = text_read_uint,
    .read_float = text_read_float,
    .read_string = text_read_string,
    .read_bool = text_read_bool,
    .read_list_begin = text_read_list_begin,
    .read_list_end = text_read_list_end,
    .read_struct_begin = text_read_struct_begin,
    .read_struct_key = text_read_struct_key,
    .read_struct_end = text_read_struct_end,
  };

  olib_serializer_t* serializer = olib_serializer_new(&config);
  if (!serializer) {
    olib_free(ctx);
    return NULL;
  }

  return serializer;
}
