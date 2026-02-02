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
#include <math.h>

// #############################################################################
// Context structure for JSON text serialization
// #############################################################################

#define JSON_INDENT_SPACES 2

typedef struct {
  // Write mode
  char* write_buffer;
  size_t write_capacity;
  size_t write_size;
  int indent_level;

  // State stack for nested containers
  int container_stack[64];  // 0 = none, 1 = list, 2 = struct
  bool first_item_stack[64];
  int stack_depth;

  const char* pending_key;

  // Read mode (using shared parsing utilities)
  text_parse_ctx_t parse;
} json_ctx_t;

// #############################################################################
// Write helpers
// #############################################################################

static bool json_ensure_write_capacity(json_ctx_t* ctx, size_t needed) {
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

static bool json_write_str(json_ctx_t* ctx, const char* str) {
  size_t len = strlen(str);
  if (!json_ensure_write_capacity(ctx, len)) return false;
  memcpy(ctx->write_buffer + ctx->write_size, str, len);
  ctx->write_size += len;
  return true;
}

static bool json_write_char(json_ctx_t* ctx, char c) {
  if (!json_ensure_write_capacity(ctx, 1)) return false;
  ctx->write_buffer[ctx->write_size++] = c;
  return true;
}

static bool json_write_indent(json_ctx_t* ctx) {
  int spaces = ctx->indent_level * JSON_INDENT_SPACES;
  if (!json_ensure_write_capacity(ctx, spaces)) return false;
  for (int i = 0; i < spaces; i++) {
    ctx->write_buffer[ctx->write_size++] = ' ';
  }
  return true;
}

static bool json_write_newline_indent(json_ctx_t* ctx) {
  if (!json_write_char(ctx, '\n')) return false;
  return json_write_indent(ctx);
}

static int json_get_container_type(json_ctx_t* ctx) {
  if (ctx->stack_depth <= 0) return 0;
  return ctx->container_stack[ctx->stack_depth - 1];
}

static bool json_is_first_item(json_ctx_t* ctx) {
  if (ctx->stack_depth <= 0) return true;
  return ctx->first_item_stack[ctx->stack_depth - 1];
}

static void json_set_first_item(json_ctx_t* ctx, bool value) {
  if (ctx->stack_depth > 0) {
    ctx->first_item_stack[ctx->stack_depth - 1] = value;
  }
}

static bool json_write_comma_if_needed(json_ctx_t* ctx) {
  if (!json_is_first_item(ctx)) {
    if (!json_write_char(ctx, ',')) return false;
  }
  json_set_first_item(ctx, false);
  return true;
}

static bool json_write_key_prefix(json_ctx_t* ctx) {
  int container = json_get_container_type(ctx);

  if (container == 2 && ctx->pending_key) {
    // Inside struct: write "key":
    if (!json_write_char(ctx, '"')) return false;
    if (!json_write_str(ctx, ctx->pending_key)) return false;
    if (!json_write_str(ctx, "\": ")) return false;
    ctx->pending_key = NULL;
  }
  return true;
}

static bool json_write_value_prefix(json_ctx_t* ctx) {
  int container = json_get_container_type(ctx);

  if (container == 1) {
    // Inside list
    if (!json_write_comma_if_needed(ctx)) return false;
    if (!json_write_newline_indent(ctx)) return false;
  } else if (container == 2) {
    // Inside struct
    if (!json_write_comma_if_needed(ctx)) return false;
    if (!json_write_newline_indent(ctx)) return false;
    if (!json_write_key_prefix(ctx)) return false;
  }
  return true;
}

// Write a JSON-escaped string (with surrounding quotes)
static bool json_write_escaped_string(json_ctx_t* ctx, const char* value) {
  if (!json_write_char(ctx, '"')) return false;

  if (value) {
    for (const char* p = value; *p; p++) {
      unsigned char c = (unsigned char)*p;
      switch (c) {
        case '"':
          if (!json_write_str(ctx, "\\\"")) return false;
          break;
        case '\\':
          if (!json_write_str(ctx, "\\\\")) return false;
          break;
        case '\b':
          if (!json_write_str(ctx, "\\b")) return false;
          break;
        case '\f':
          if (!json_write_str(ctx, "\\f")) return false;
          break;
        case '\n':
          if (!json_write_str(ctx, "\\n")) return false;
          break;
        case '\r':
          if (!json_write_str(ctx, "\\r")) return false;
          break;
        case '\t':
          if (!json_write_str(ctx, "\\t")) return false;
          break;
        default:
          if (c < 0x20) {
            // Control characters: use \uXXXX escape
            char buf[8];
            snprintf(buf, sizeof(buf), "\\u%04x", c);
            if (!json_write_str(ctx, buf)) return false;
          } else {
            if (!json_write_char(ctx, c)) return false;
          }
          break;
      }
    }
  }

  if (!json_write_char(ctx, '"')) return false;
  return true;
}

// #############################################################################
// Write callbacks
// #############################################################################

static bool json_write_int(void* ctx, int64_t value) {
  json_ctx_t* c = (json_ctx_t*)ctx;

  if (!json_write_value_prefix(c)) return false;

  char buf[32];
  snprintf(buf, sizeof(buf), "%lld", (long long)value);
  return json_write_str(c, buf);
}

static bool json_write_uint(void* ctx, uint64_t value) {
  json_ctx_t* c = (json_ctx_t*)ctx;

  if (!json_write_value_prefix(c)) return false;

  char buf[32];
  snprintf(buf, sizeof(buf), "%llu", (unsigned long long)value);
  return json_write_str(c, buf);
}

static bool json_write_float(void* ctx, double value) {
  json_ctx_t* c = (json_ctx_t*)ctx;

  if (!json_write_value_prefix(c)) return false;

  char buf[64];
  // Handle special float values (JSON doesn't support Infinity/NaN)
  if (isnan(value)) {
    snprintf(buf, sizeof(buf), "null");
  } else if (isinf(value)) {
    snprintf(buf, sizeof(buf), "null");
  } else {
    snprintf(buf, sizeof(buf), "%.17g", value);
    // Ensure the output looks like a floating point number
    bool has_decimal = false;
    for (char* p = buf; *p; p++) {
      if (*p == '.' || *p == 'e' || *p == 'E') {
        has_decimal = true;
        break;
      }
    }
    if (!has_decimal) {
      strcat(buf, ".0");
    }
  }
  return json_write_str(c, buf);
}

static bool json_write_string(void* ctx, const char* value) {
  json_ctx_t* c = (json_ctx_t*)ctx;

  if (!json_write_value_prefix(c)) return false;

  return json_write_escaped_string(c, value);
}

static bool json_write_bool(void* ctx, bool value) {
  json_ctx_t* c = (json_ctx_t*)ctx;

  if (!json_write_value_prefix(c)) return false;

  return json_write_str(c, value ? "true" : "false");
}

static bool json_write_list_begin(void* ctx, size_t size) {
  (void)size;
  json_ctx_t* c = (json_ctx_t*)ctx;

  if (!json_write_value_prefix(c)) return false;

  if (!json_write_char(c, '[')) return false;

  // Push list onto stack
  if (c->stack_depth < 64) {
    c->container_stack[c->stack_depth] = 1;  // list
    c->first_item_stack[c->stack_depth] = true;
    c->stack_depth++;
  }
  c->indent_level++;

  return true;
}

static bool json_write_list_end(void* ctx) {
  json_ctx_t* c = (json_ctx_t*)ctx;

  c->indent_level--;

  // Check if list was empty
  bool was_empty = json_is_first_item(c);

  // Pop from stack
  if (c->stack_depth > 0) {
    c->stack_depth--;
  }

  if (!was_empty) {
    if (!json_write_newline_indent(c)) return false;
  }

  return json_write_char(c, ']');
}

static bool json_write_struct_begin(void* ctx) {
  json_ctx_t* c = (json_ctx_t*)ctx;

  if (!json_write_value_prefix(c)) return false;

  if (!json_write_char(c, '{')) return false;

  // Push struct onto stack
  if (c->stack_depth < 64) {
    c->container_stack[c->stack_depth] = 2;  // struct
    c->first_item_stack[c->stack_depth] = true;
    c->stack_depth++;
  }
  c->indent_level++;

  return true;
}

static bool json_write_struct_key(void* ctx, const char* key) {
  json_ctx_t* c = (json_ctx_t*)ctx;
  c->pending_key = key;
  return true;
}

static bool json_write_struct_end(void* ctx) {
  json_ctx_t* c = (json_ctx_t*)ctx;

  c->indent_level--;

  // Check if struct was empty
  bool was_empty = json_is_first_item(c);

  // Pop from stack
  if (c->stack_depth > 0) {
    c->stack_depth--;
  }

  if (!was_empty) {
    if (!json_write_newline_indent(c)) return false;
  }

  return json_write_char(c, '}');
}

// #############################################################################
// Read helpers
// #############################################################################

// Skip JSON whitespace (space, tab, newline, carriage return)
static void json_skip_whitespace(text_parse_ctx_t* p) {
  while (p->pos < p->size) {
    char c = p->buffer[p->pos];
    if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
      p->pos++;
    } else {
      break;
    }
  }
}

// Parse a JSON string, handling escape sequences
static const char* json_parse_string(text_parse_ctx_t* p) {
  json_skip_whitespace(p);

  if (p->pos >= p->size || p->buffer[p->pos] != '"') {
    return NULL;
  }
  p->pos++;  // Skip opening quote

  // First pass: calculate length
  size_t start = p->pos;
  size_t len = 0;
  while (p->pos < p->size && p->buffer[p->pos] != '"') {
    if (p->buffer[p->pos] == '\\' && p->pos + 1 < p->size) {
      p->pos++;
      char esc = p->buffer[p->pos];
      if (esc == 'u') {
        // Unicode escape: \uXXXX
        p->pos += 4;  // Skip 4 hex digits
        len++;  // Simplified: treat as single char
      } else {
        len++;
      }
    } else {
      len++;
    }
    p->pos++;
  }

  if (p->pos >= p->size) {
    return NULL;  // Unterminated string
  }

  // Allocate temp string
  if (!text_parse_ensure_temp(p, len)) {
    return NULL;
  }

  // Second pass: copy with escape handling
  p->pos = start;
  size_t out = 0;
  while (p->pos < p->size && p->buffer[p->pos] != '"') {
    if (p->buffer[p->pos] == '\\' && p->pos + 1 < p->size) {
      p->pos++;
      char esc = p->buffer[p->pos];
      switch (esc) {
        case '"':  p->temp_string[out++] = '"'; break;
        case '\\': p->temp_string[out++] = '\\'; break;
        case '/':  p->temp_string[out++] = '/'; break;
        case 'b':  p->temp_string[out++] = '\b'; break;
        case 'f':  p->temp_string[out++] = '\f'; break;
        case 'n':  p->temp_string[out++] = '\n'; break;
        case 'r':  p->temp_string[out++] = '\r'; break;
        case 't':  p->temp_string[out++] = '\t'; break;
        case 'u': {
          // Unicode escape: \uXXXX - simplified handling
          if (p->pos + 4 < p->size) {
            char hex[5] = {p->buffer[p->pos+1], p->buffer[p->pos+2],
                           p->buffer[p->pos+3], p->buffer[p->pos+4], 0};
            unsigned int code = (unsigned int)strtoul(hex, NULL, 16);
            if (code < 0x80) {
              p->temp_string[out++] = (char)code;
            } else if (code < 0x800) {
              p->temp_string[out++] = (char)(0xC0 | (code >> 6));
              p->temp_string[out++] = (char)(0x80 | (code & 0x3F));
            } else {
              p->temp_string[out++] = (char)(0xE0 | (code >> 12));
              p->temp_string[out++] = (char)(0x80 | ((code >> 6) & 0x3F));
              p->temp_string[out++] = (char)(0x80 | (code & 0x3F));
            }
            p->pos += 4;
          }
          break;
        }
        default:
          p->temp_string[out++] = esc;
          break;
      }
    } else {
      p->temp_string[out++] = p->buffer[p->pos];
    }
    p->pos++;
  }
  p->temp_string[out] = '\0';

  if (p->pos < p->size && p->buffer[p->pos] == '"') {
    p->pos++;  // Skip closing quote
  }

  return p->temp_string;
}

// Parse a JSON number
static bool json_parse_number(text_parse_ctx_t* p, text_parse_number_result_t* result) {
  json_skip_whitespace(p);

  if (p->pos >= p->size) return false;

  size_t start = p->pos;
  bool is_negative = false;
  bool is_float = false;

  // Optional minus
  if (p->buffer[p->pos] == '-') {
    is_negative = true;
    p->pos++;
  }

  // Integer part
  if (p->pos >= p->size || !isdigit((unsigned char)p->buffer[p->pos])) {
    p->pos = start;
    return false;
  }

  while (p->pos < p->size && isdigit((unsigned char)p->buffer[p->pos])) {
    p->pos++;
  }

  // Fractional part
  if (p->pos < p->size && p->buffer[p->pos] == '.') {
    is_float = true;
    p->pos++;
    while (p->pos < p->size && isdigit((unsigned char)p->buffer[p->pos])) {
      p->pos++;
    }
  }

  // Exponent part
  if (p->pos < p->size && (p->buffer[p->pos] == 'e' || p->buffer[p->pos] == 'E')) {
    is_float = true;
    p->pos++;
    if (p->pos < p->size && (p->buffer[p->pos] == '+' || p->buffer[p->pos] == '-')) {
      p->pos++;
    }
    while (p->pos < p->size && isdigit((unsigned char)p->buffer[p->pos])) {
      p->pos++;
    }
  }

  // Parse the number
  size_t len = p->pos - start;
  char* buf = olib_malloc(len + 1);
  if (!buf) return false;
  memcpy(buf, p->buffer + start, len);
  buf[len] = '\0';

  result->is_float = is_float;
  result->is_negative = is_negative;

  if (is_float) {
    result->float_value = strtod(buf, NULL);
    result->int_value = (int64_t)result->float_value;
  } else {
    result->int_value = strtoll(buf, NULL, 10);
    result->float_value = (double)result->int_value;
  }

  olib_free(buf);
  return true;
}

// #############################################################################
// Read callbacks
// #############################################################################

static olib_object_type_t json_read_peek(void* ctx) {
  json_ctx_t* c = (json_ctx_t*)ctx;
  text_parse_ctx_t* p = &c->parse;
  json_skip_whitespace(p);

  // Skip comma if present (between list/object elements)
  if (p->pos < p->size && p->buffer[p->pos] == ',') {
    p->pos++;
    json_skip_whitespace(p);
  }

  if (p->pos >= p->size) {
    return OLIB_OBJECT_TYPE_MAX;
  }

  char ch = p->buffer[p->pos];

  if (ch == '"') {
    return OLIB_OBJECT_TYPE_STRING;
  }
  if (ch == '{') {
    return OLIB_OBJECT_TYPE_STRUCT;
  }
  if (ch == '[') {
    return OLIB_OBJECT_TYPE_LIST;
  }
  if (ch == '-' || isdigit((unsigned char)ch)) {
    // Look ahead to determine if int or float
    size_t pos = p->pos;
    if (ch == '-') pos++;
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
  if (ch == 'n') {
    // null - treat as a special case, return as int with value 0
    if (p->pos + 4 <= p->size && strncmp(p->buffer + p->pos, "null", 4) == 0) {
      return OLIB_OBJECT_TYPE_INT;
    }
  }

  return OLIB_OBJECT_TYPE_MAX;
}

static bool json_read_int(void* ctx, int64_t* value) {
  json_ctx_t* c = (json_ctx_t*)ctx;
  text_parse_ctx_t* p = &c->parse;
  json_skip_whitespace(p);

  // Handle null
  if (p->pos + 4 <= p->size && strncmp(p->buffer + p->pos, "null", 4) == 0) {
    p->pos += 4;
    *value = 0;
    return true;
  }

  text_parse_number_result_t result;
  if (!json_parse_number(p, &result)) return false;
  *value = result.int_value;
  return true;
}

static bool json_read_uint(void* ctx, uint64_t* value) {
  json_ctx_t* c = (json_ctx_t*)ctx;
  text_parse_ctx_t* p = &c->parse;
  json_skip_whitespace(p);

  // Handle null
  if (p->pos + 4 <= p->size && strncmp(p->buffer + p->pos, "null", 4) == 0) {
    p->pos += 4;
    *value = 0;
    return true;
  }

  text_parse_number_result_t result;
  if (!json_parse_number(p, &result)) return false;
  *value = (uint64_t)result.int_value;
  return true;
}

static bool json_read_float(void* ctx, double* value) {
  json_ctx_t* c = (json_ctx_t*)ctx;
  text_parse_ctx_t* p = &c->parse;
  json_skip_whitespace(p);

  // Handle null (treat as NaN or 0)
  if (p->pos + 4 <= p->size && strncmp(p->buffer + p->pos, "null", 4) == 0) {
    p->pos += 4;
    *value = 0.0;
    return true;
  }

  text_parse_number_result_t result;
  if (!json_parse_number(p, &result)) return false;
  *value = result.is_float ? result.float_value : (double)result.int_value;
  return true;
}

static bool json_read_string(void* ctx, const char** value) {
  json_ctx_t* c = (json_ctx_t*)ctx;
  const char* str = json_parse_string(&c->parse);
  if (!str) return false;
  *value = str;
  return true;
}

static bool json_read_bool(void* ctx, bool* value) {
  json_ctx_t* c = (json_ctx_t*)ctx;
  text_parse_ctx_t* p = &c->parse;
  json_skip_whitespace(p);

  if (p->pos + 4 <= p->size && strncmp(p->buffer + p->pos, "true", 4) == 0) {
    p->pos += 4;
    *value = true;
    return true;
  }
  if (p->pos + 5 <= p->size && strncmp(p->buffer + p->pos, "false", 5) == 0) {
    p->pos += 5;
    *value = false;
    return true;
  }
  return false;
}

static bool json_read_list_begin(void* ctx, size_t* size) {
  json_ctx_t* c = (json_ctx_t*)ctx;
  text_parse_ctx_t* p = &c->parse;
  json_skip_whitespace(p);

  if (p->pos >= p->size || p->buffer[p->pos] != '[') {
    return false;
  }
  p->pos++;  // Skip '['

  // Count elements by scanning ahead (without consuming)
  size_t saved_pos = p->pos;
  size_t count = 0;
  int depth = 1;
  bool in_string = false;
  bool has_content = false;

  while (saved_pos < p->size && depth > 0) {
    char ch = p->buffer[saved_pos];

    if (in_string) {
      if (ch == '\\' && saved_pos + 1 < p->size) {
        saved_pos += 2;
        continue;
      }
      if (ch == '"') {
        in_string = false;
      }
    } else {
      if (ch == '"') {
        in_string = true;
        has_content = true;
      } else if (ch == '[' || ch == '{') {
        depth++;
        has_content = true;
      } else if (ch == ']' || ch == '}') {
        depth--;
      } else if (depth == 1 && ch == ',') {
        count++;
      } else if (ch != ' ' && ch != '\t' && ch != '\n' && ch != '\r' && depth == 1) {
        has_content = true;
      }
    }
    saved_pos++;
  }

  if (has_content) {
    count++;
  }

  *size = count;
  return true;
}

static bool json_read_list_end(void* ctx) {
  json_ctx_t* c = (json_ctx_t*)ctx;
  text_parse_ctx_t* p = &c->parse;
  json_skip_whitespace(p);

  // Skip optional comma
  if (p->pos < p->size && p->buffer[p->pos] == ',') {
    p->pos++;
    json_skip_whitespace(p);
  }

  if (p->pos >= p->size || p->buffer[p->pos] != ']') {
    return false;
  }
  p->pos++;  // Skip ']'
  return true;
}

static bool json_read_struct_begin(void* ctx) {
  json_ctx_t* c = (json_ctx_t*)ctx;
  text_parse_ctx_t* p = &c->parse;
  json_skip_whitespace(p);

  if (p->pos >= p->size || p->buffer[p->pos] != '{') {
    return false;
  }
  p->pos++;  // Skip '{'
  return true;
}

static bool json_read_struct_key(void* ctx, const char** key) {
  json_ctx_t* c = (json_ctx_t*)ctx;
  text_parse_ctx_t* p = &c->parse;
  json_skip_whitespace(p);

  // Skip comma if present
  if (p->pos < p->size && p->buffer[p->pos] == ',') {
    p->pos++;
    json_skip_whitespace(p);
  }

  // Check if we've reached the end of the struct
  if (p->pos >= p->size || p->buffer[p->pos] == '}') {
    return false;
  }

  // Read key (must be a string)
  const char* k = json_parse_string(p);
  if (!k) return false;

  // Skip colon
  json_skip_whitespace(p);
  if (p->pos >= p->size || p->buffer[p->pos] != ':') {
    return false;
  }
  p->pos++;  // Skip ':'

  *key = k;
  return true;
}

static bool json_read_struct_end(void* ctx) {
  json_ctx_t* c = (json_ctx_t*)ctx;
  text_parse_ctx_t* p = &c->parse;
  json_skip_whitespace(p);

  if (p->pos >= p->size || p->buffer[p->pos] != '}') {
    return false;
  }
  p->pos++;  // Skip '}'
  return true;
}

// #############################################################################
// Lifecycle callbacks
// #############################################################################

static void json_free_ctx(void* ctx) {
  json_ctx_t* c = (json_ctx_t*)ctx;
  if (c->write_buffer) olib_free(c->write_buffer);
  text_parse_free(&c->parse);
  olib_free(c);
}

static bool json_init_write(void* ctx) {
  json_ctx_t* c = (json_ctx_t*)ctx;
  c->write_size = 0;
  c->indent_level = 0;
  c->stack_depth = 0;
  c->pending_key = NULL;
  return true;
}

static bool json_finish_write(void* ctx, uint8_t** out_data, size_t* out_size) {
  json_ctx_t* c = (json_ctx_t*)ctx;
  if (!out_data || !out_size) {
    return false;
  }

  // Add newline and null terminator
  if (!json_ensure_write_capacity(c, 2)) return false;
  c->write_buffer[c->write_size++] = '\n';
  c->write_buffer[c->write_size] = '\0';

  *out_data = (uint8_t*)c->write_buffer;
  *out_size = c->write_size;

  c->write_buffer = NULL;
  c->write_capacity = 0;
  c->write_size = 0;
  return true;
}

static bool json_init_read(void* ctx, const uint8_t* data, size_t size) {
  json_ctx_t* c = (json_ctx_t*)ctx;
  text_parse_init(&c->parse, (const char*)data, size);
  return true;
}

static bool json_finish_read(void* ctx) {
  json_ctx_t* c = (json_ctx_t*)ctx;
  text_parse_reset(&c->parse);
  return true;
}

// #############################################################################
// Public API
// #############################################################################

OLIB_API olib_serializer_t* olib_serializer_new_json_text() {
  json_ctx_t* ctx = olib_calloc(1, sizeof(json_ctx_t));
  if (!ctx) {
    return NULL;
  }

  olib_serializer_config_t config = {
    .user_data = ctx,
    .text_based = true,
    .free_ctx = json_free_ctx,
    .init_write = json_init_write,
    .finish_write = json_finish_write,
    .init_read = json_init_read,
    .finish_read = json_finish_read,

    .write_int = json_write_int,
    .write_uint = json_write_uint,
    .write_float = json_write_float,
    .write_string = json_write_string,
    .write_bool = json_write_bool,
    .write_list_begin = json_write_list_begin,
    .write_list_end = json_write_list_end,
    .write_struct_begin = json_write_struct_begin,
    .write_struct_key = json_write_struct_key,
    .write_struct_end = json_write_struct_end,

    .read_peek = json_read_peek,
    .read_int = json_read_int,
    .read_uint = json_read_uint,
    .read_float = json_read_float,
    .read_string = json_read_string,
    .read_bool = json_read_bool,
    .read_list_begin = json_read_list_begin,
    .read_list_end = json_read_list_end,
    .read_struct_begin = json_read_struct_begin,
    .read_struct_key = json_read_struct_key,
    .read_struct_end = json_read_struct_end,
  };

  olib_serializer_t* serializer = olib_serializer_new(&config);
  if (!serializer) {
    olib_free(ctx);
    return NULL;
  }

  return serializer;
}
