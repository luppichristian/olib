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
// Context structure for TOML serialization
// #############################################################################

typedef struct {
  // Write mode
  char* write_buffer;
  size_t write_capacity;
  size_t write_size;
  int nesting_level;       // Track nesting depth (0 = top-level table)
  bool in_list;
  bool list_first_item;
  bool in_inline_table;    // Inside inline table { }
  bool inline_first_item;
  const char* pending_key;

  // Read mode (using shared parsing utilities)
  text_parse_ctx_t parse;
} toml_ctx_t;

// #############################################################################
// Write helpers
// #############################################################################

static bool toml_ensure_write_capacity(toml_ctx_t* ctx, size_t needed) {
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

static bool toml_write_str(toml_ctx_t* ctx, const char* str) {
  size_t len = strlen(str);
  if (!toml_ensure_write_capacity(ctx, len)) return false;
  memcpy(ctx->write_buffer + ctx->write_size, str, len);
  ctx->write_size += len;
  return true;
}

static bool toml_write_char(toml_ctx_t* ctx, char c) {
  if (!toml_ensure_write_capacity(ctx, 1)) return false;
  ctx->write_buffer[ctx->write_size++] = c;
  return true;
}

// Check if a key is a valid bare key (alphanumeric + underscore + hyphen)
static bool toml_is_bare_key(const char* key) {
  if (!key || !*key) return false;
  for (const char* p = key; *p; p++) {
    char c = *p;
    if (!((c >= 'a' && c <= 'z') ||
          (c >= 'A' && c <= 'Z') ||
          (c >= '0' && c <= '9') ||
          c == '_' || c == '-')) {
      return false;
    }
  }
  return true;
}

// Write a key (bare or quoted)
static bool toml_write_key(toml_ctx_t* ctx, const char* key) {
  if (toml_is_bare_key(key)) {
    return toml_write_str(ctx, key);
  }
  // Quoted key
  if (!toml_write_char(ctx, '"')) return false;
  for (const char* p = key; *p; p++) {
    switch (*p) {
      case '"':
        if (!toml_write_str(ctx, "\\\"")) return false;
        break;
      case '\\':
        if (!toml_write_str(ctx, "\\\\")) return false;
        break;
      case '\n':
        if (!toml_write_str(ctx, "\\n")) return false;
        break;
      case '\r':
        if (!toml_write_str(ctx, "\\r")) return false;
        break;
      case '\t':
        if (!toml_write_str(ctx, "\\t")) return false;
        break;
      default:
        if (!toml_write_char(ctx, *p)) return false;
        break;
    }
  }
  return toml_write_char(ctx, '"');
}

// Write key prefix with " = " for inline or newline for top-level
static bool toml_write_key_prefix(toml_ctx_t* ctx) {
  if (ctx->pending_key) {
    if (!toml_write_key(ctx, ctx->pending_key)) return false;
    if (!toml_write_str(ctx, " = ")) return false;
    ctx->pending_key = NULL;
  }
  return true;
}

// Write separator for list items or inline table items
static bool toml_write_item_separator(toml_ctx_t* ctx) {
  if (ctx->in_list) {
    if (!ctx->list_first_item) {
      if (!toml_write_str(ctx, ", ")) return false;
    }
    ctx->list_first_item = false;
  } else if (ctx->in_inline_table) {
    if (!ctx->inline_first_item) {
      if (!toml_write_str(ctx, ", ")) return false;
    }
    ctx->inline_first_item = false;
  }
  return true;
}

// #############################################################################
// Write callbacks
// #############################################################################

static bool toml_write_int(void* ctx, int64_t value) {
  toml_ctx_t* c = (toml_ctx_t*)ctx;

  if (!toml_write_item_separator(c)) return false;
  if (!toml_write_key_prefix(c)) return false;

  char buf[32];
  snprintf(buf, sizeof(buf), "%lld", (long long)value);
  if (!toml_write_str(c, buf)) return false;

  // Add newline if at top-level table
  if (c->nesting_level == 1 && !c->in_list && !c->in_inline_table) {
    if (!toml_write_char(c, '\n')) return false;
  }

  return true;
}

static bool toml_write_uint(void* ctx, uint64_t value) {
  toml_ctx_t* c = (toml_ctx_t*)ctx;

  if (!toml_write_item_separator(c)) return false;
  if (!toml_write_key_prefix(c)) return false;

  char buf[32];
  snprintf(buf, sizeof(buf), "%llu", (unsigned long long)value);
  if (!toml_write_str(c, buf)) return false;

  // Add newline if at top-level table
  if (c->nesting_level == 1 && !c->in_list && !c->in_inline_table) {
    if (!toml_write_char(c, '\n')) return false;
  }

  return true;
}

static bool toml_write_float(void* ctx, double value) {
  toml_ctx_t* c = (toml_ctx_t*)ctx;

  if (!toml_write_item_separator(c)) return false;
  if (!toml_write_key_prefix(c)) return false;

  char buf[64];
  // TOML requires a decimal point for floats, use %g but ensure decimal point
  snprintf(buf, sizeof(buf), "%g", value);
  // If there's no decimal point or exponent, add .0
  bool has_decimal = false;
  for (char* p = buf; *p; p++) {
    if (*p == '.' || *p == 'e' || *p == 'E') {
      has_decimal = true;
      break;
    }
  }
  if (!toml_write_str(c, buf)) return false;
  if (!has_decimal) {
    if (!toml_write_str(c, ".0")) return false;
  }

  // Add newline if at top-level table
  if (c->nesting_level == 1 && !c->in_list && !c->in_inline_table) {
    if (!toml_write_char(c, '\n')) return false;
  }

  return true;
}

static bool toml_write_string(void* ctx, const char* value) {
  toml_ctx_t* c = (toml_ctx_t*)ctx;

  if (!toml_write_item_separator(c)) return false;
  if (!toml_write_key_prefix(c)) return false;

  // Write basic string with escapes
  if (!toml_write_char(c, '"')) return false;
  if (value) {
    for (const char* p = value; *p; p++) {
      switch (*p) {
        case '"':
          if (!toml_write_str(c, "\\\"")) return false;
          break;
        case '\\':
          if (!toml_write_str(c, "\\\\")) return false;
          break;
        case '\n':
          if (!toml_write_str(c, "\\n")) return false;
          break;
        case '\r':
          if (!toml_write_str(c, "\\r")) return false;
          break;
        case '\t':
          if (!toml_write_str(c, "\\t")) return false;
          break;
        case '\b':
          if (!toml_write_str(c, "\\b")) return false;
          break;
        case '\f':
          if (!toml_write_str(c, "\\f")) return false;
          break;
        default:
          if ((unsigned char)*p < 0x20) {
            // Control character - use \uXXXX
            char escape[8];
            snprintf(escape, sizeof(escape), "\\u%04x", (unsigned char)*p);
            if (!toml_write_str(c, escape)) return false;
          } else {
            if (!toml_write_char(c, *p)) return false;
          }
          break;
      }
    }
  }
  if (!toml_write_char(c, '"')) return false;

  // Add newline if at top-level table
  if (c->nesting_level == 1 && !c->in_list && !c->in_inline_table) {
    if (!toml_write_char(c, '\n')) return false;
  }

  return true;
}

static bool toml_write_bool(void* ctx, bool value) {
  toml_ctx_t* c = (toml_ctx_t*)ctx;

  if (!toml_write_item_separator(c)) return false;
  if (!toml_write_key_prefix(c)) return false;

  if (!toml_write_str(c, value ? "true" : "false")) return false;

  // Add newline if at top-level table
  if (c->nesting_level == 1 && !c->in_list && !c->in_inline_table) {
    if (!toml_write_char(c, '\n')) return false;
  }

  return true;
}

static bool toml_write_list_begin(void* ctx, size_t size) {
  (void)size;
  toml_ctx_t* c = (toml_ctx_t*)ctx;

  if (!toml_write_item_separator(c)) return false;
  if (!toml_write_key_prefix(c)) return false;

  if (!toml_write_char(c, '[')) return false;
  c->in_list = true;
  c->list_first_item = true;
  return true;
}

static bool toml_write_list_end(void* ctx) {
  toml_ctx_t* c = (toml_ctx_t*)ctx;
  if (!toml_write_char(c, ']')) return false;
  c->in_list = false;

  // Add newline if at top-level table
  if (c->nesting_level == 1 && !c->in_inline_table) {
    if (!toml_write_char(c, '\n')) return false;
  }

  return true;
}

static bool toml_write_struct_begin(void* ctx) {
  toml_ctx_t* c = (toml_ctx_t*)ctx;

  // If we're at nesting level 0 (root) or already in inline context, use inline table
  // If nesting level 1 and not inline, this is a top-level table (section header)
  // If nesting level > 1, we must use inline tables

  if (c->in_list) {
    // Struct inside list - must be inline
    if (!c->list_first_item) {
      if (!toml_write_str(c, ", ")) return false;
    }
    c->list_first_item = false;
    if (!toml_write_char(c, '{')) return false;
    c->in_inline_table = true;
    c->inline_first_item = true;
    c->nesting_level++;
    return true;
  }

  if (c->in_inline_table) {
    // Nested struct inside inline table
    if (!c->inline_first_item) {
      if (!toml_write_str(c, ", ")) return false;
    }
    c->inline_first_item = false;
    if (!toml_write_key_prefix(c)) return false;
    if (!toml_write_char(c, '{')) return false;
    c->nesting_level++;
    c->inline_first_item = true;
    return true;
  }

  if (c->nesting_level == 0) {
    // Root level struct - this becomes the document
    c->nesting_level++;
    return true;
  }

  // Nested struct at top-level - use inline table
  if (!toml_write_key_prefix(c)) return false;
  if (!toml_write_char(c, '{')) return false;
  c->in_inline_table = true;
  c->inline_first_item = true;
  c->nesting_level++;
  return true;
}

static bool toml_write_struct_key(void* ctx, const char* key) {
  toml_ctx_t* c = (toml_ctx_t*)ctx;
  c->pending_key = key;
  return true;
}

static bool toml_write_struct_end(void* ctx) {
  toml_ctx_t* c = (toml_ctx_t*)ctx;
  c->nesting_level--;

  if (c->in_inline_table && c->nesting_level >= 1) {
    if (!toml_write_char(c, '}')) return false;
    // Check if we're exiting the inline table
    if (c->nesting_level == 1) {
      c->in_inline_table = false;
      if (!c->in_list) {
        if (!toml_write_char(c, '\n')) return false;
      }
    }
    return true;
  }

  if (c->in_inline_table) {
    if (!toml_write_char(c, '}')) return false;
    c->in_inline_table = false;
    return true;
  }

  // Top-level struct end - nothing to write
  return true;
}

static bool toml_write_matrix(void* ctx, size_t ndims, const size_t* dims, const double* data) {
  toml_ctx_t* c = (toml_ctx_t*)ctx;

  if (!toml_write_item_separator(c)) return false;
  if (!toml_write_key_prefix(c)) return false;

  // Write matrix as inline table: { dims = [d1, d2, ...], data = [v1, v2, ...] }
  if (!toml_write_str(c, "{ dims = [")) return false;

  for (size_t i = 0; i < ndims; i++) {
    if (i > 0) {
      if (!toml_write_str(c, ", ")) return false;
    }
    char buf[32];
    snprintf(buf, sizeof(buf), "%zu", dims[i]);
    if (!toml_write_str(c, buf)) return false;
  }

  if (!toml_write_str(c, "], data = [")) return false;

  size_t total = 1;
  for (size_t i = 0; i < ndims; i++) {
    total *= dims[i];
  }

  for (size_t i = 0; i < total; i++) {
    if (i > 0) {
      if (!toml_write_str(c, ", ")) return false;
    }
    char buf[64];
    snprintf(buf, sizeof(buf), "%g", data[i]);
    if (!toml_write_str(c, buf)) return false;
  }

  if (!toml_write_str(c, "] }")) return false;

  // Add newline if at top-level table
  if (c->nesting_level == 1 && !c->in_list && !c->in_inline_table) {
    if (!toml_write_char(c, '\n')) return false;
  }

  return true;
}

// #############################################################################
// Read helpers
// #############################################################################

// Skip TOML comments (# to end of line)
static void toml_skip_whitespace_and_comments(text_parse_ctx_t* p) {
  text_parse_skip_whitespace_and_comments(p);
}

// Parse a TOML key (bare key or quoted key)
static const char* toml_parse_key(text_parse_ctx_t* p) {
  text_parse_skip_whitespace(p);

  char ch = text_parse_peek_raw(p);

  // Quoted key (basic string)
  if (ch == '"') {
    return text_parse_quoted_string(p);
  }

  // Quoted key (literal string)
  if (ch == '\'') {
    return text_parse_single_quoted_string(p);
  }

  // Bare key
  return text_parse_identifier(p);
}

// Parse a TOML literal string (single quotes, no escapes except '' for ')
static const char* toml_parse_literal_string(text_parse_ctx_t* p) {
  return text_parse_single_quoted_string(p);
}

// #############################################################################
// Read callbacks
// #############################################################################

static olib_object_type_t toml_read_peek(void* ctx) {
  toml_ctx_t* c = (toml_ctx_t*)ctx;
  text_parse_ctx_t* p = &c->parse;
  toml_skip_whitespace_and_comments(p);

  // Skip comma if present (between list/inline table elements)
  if (p->pos < p->size && p->buffer[p->pos] == ',') {
    p->pos++;
    toml_skip_whitespace_and_comments(p);
  }

  if (text_parse_eof(p)) {
    return OLIB_OBJECT_TYPE_MAX;
  }

  char ch = text_parse_peek_raw(p);

  // String (basic or literal)
  if (ch == '"' || ch == '\'') {
    return OLIB_OBJECT_TYPE_STRING;
  }

  // Inline table
  if (ch == '{') {
    return OLIB_OBJECT_TYPE_STRUCT;
  }

  // Array
  if (ch == '[') {
    return OLIB_OBJECT_TYPE_LIST;
  }

  // Number
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

  // Boolean
  if (ch == 't') {
    if (p->pos + 4 <= p->size && strncmp(p->buffer + p->pos, "true", 4) == 0) {
      // Make sure it's not part of a longer identifier
      if (p->pos + 4 >= p->size || !text_parse_is_identifier_char(p->buffer[p->pos + 4])) {
        return OLIB_OBJECT_TYPE_BOOL;
      }
    }
  }
  if (ch == 'f') {
    if (p->pos + 5 <= p->size && strncmp(p->buffer + p->pos, "false", 5) == 0) {
      if (p->pos + 5 >= p->size || !text_parse_is_identifier_char(p->buffer[p->pos + 5])) {
        return OLIB_OBJECT_TYPE_BOOL;
      }
    }
  }

  // Check if this looks like a key = value pair (implicit struct/table)
  // This handles the case where a top-level struct is serialized as key-value pairs
  if (text_parse_is_identifier_char(ch)) {
    size_t saved_pos = p->pos;
    // Try to parse a key
    const char* key = toml_parse_key(p);
    if (key) {
      toml_skip_whitespace_and_comments(p);
      if (p->pos < p->size && p->buffer[p->pos] == '=') {
        // This is a key=value pair, indicating a struct
        p->pos = saved_pos;
        return OLIB_OBJECT_TYPE_STRUCT;
      }
    }
    p->pos = saved_pos;
  }

  return OLIB_OBJECT_TYPE_MAX;
}

static bool toml_read_int(void* ctx, int64_t* value) {
  toml_ctx_t* c = (toml_ctx_t*)ctx;
  text_parse_number_result_t result;
  if (!text_parse_number(&c->parse, &result)) return false;
  *value = result.int_value;
  return true;
}

static bool toml_read_uint(void* ctx, uint64_t* value) {
  toml_ctx_t* c = (toml_ctx_t*)ctx;
  text_parse_number_result_t result;
  if (!text_parse_number(&c->parse, &result)) return false;
  *value = (uint64_t)result.int_value;
  return true;
}

static bool toml_read_float(void* ctx, double* value) {
  toml_ctx_t* c = (toml_ctx_t*)ctx;
  text_parse_number_result_t result;
  if (!text_parse_number(&c->parse, &result)) return false;
  *value = result.is_float ? result.float_value : (double)result.int_value;
  return true;
}

static bool toml_read_string(void* ctx, const char** value) {
  toml_ctx_t* c = (toml_ctx_t*)ctx;
  text_parse_ctx_t* p = &c->parse;
  text_parse_skip_whitespace(p);

  char ch = text_parse_peek_raw(p);

  // Basic string (double quotes with escapes)
  if (ch == '"') {
    const char* str = text_parse_quoted_string(p);
    if (!str) return false;
    *value = str;
    return true;
  }

  // Literal string (single quotes, no escapes)
  if (ch == '\'') {
    const char* str = toml_parse_literal_string(p);
    if (!str) return false;
    *value = str;
    return true;
  }

  return false;
}

static bool toml_read_bool(void* ctx, bool* value) {
  toml_ctx_t* c = (toml_ctx_t*)ctx;
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

static bool toml_read_list_begin(void* ctx, size_t* size) {
  toml_ctx_t* c = (toml_ctx_t*)ctx;
  text_parse_ctx_t* p = &c->parse;

  if (!text_parse_match(p, '[')) return false;

  // Count elements by scanning ahead (without consuming)
  size_t pos = p->pos;
  size_t count = 0;
  int depth = 1;
  bool has_content = false;
  bool in_string = false;
  char string_char = 0;

  while (pos < p->size && depth > 0) {
    char ch = p->buffer[pos];

    if (in_string) {
      if (ch == '\\' && pos + 1 < p->size) {
        pos += 2;
        continue;
      }
      if (ch == string_char) {
        in_string = false;
      }
      pos++;
      continue;
    }

    if (ch == '"' || ch == '\'') {
      in_string = true;
      string_char = ch;
      has_content = true;
    } else if (ch == '[' || ch == '{') {
      depth++;
      has_content = true;
    } else if (ch == ']') {
      depth--;
    } else if (ch == '}') {
      depth--;
    } else if (depth == 1 && ch == ',') {
      count++;
    } else if (ch == '#') {
      // Skip comment
      while (pos < p->size && p->buffer[pos] != '\n') {
        pos++;
      }
      continue;
    } else if (ch != ' ' && ch != '\t' && ch != '\n' && ch != '\r' && depth == 1) {
      has_content = true;
    }
    pos++;
  }

  if (has_content) {
    count++;
  }

  *size = count;
  c->in_list = true;
  c->list_first_item = true;
  return true;
}

static bool toml_read_list_end(void* ctx) {
  toml_ctx_t* c = (toml_ctx_t*)ctx;
  text_parse_ctx_t* p = &c->parse;
  toml_skip_whitespace_and_comments(p);
  text_parse_match(p, ',');  // Optional trailing comma
  c->in_list = false;
  return text_parse_match(p, ']');
}

static bool toml_read_struct_begin(void* ctx) {
  toml_ctx_t* c = (toml_ctx_t*)ctx;
  text_parse_ctx_t* p = &c->parse;
  toml_skip_whitespace_and_comments(p);

  // Check for inline table
  if (text_parse_peek_raw(p) == '{') {
    text_parse_match(p, '{');
    c->in_inline_table = true;
    c->inline_first_item = true;
    c->nesting_level++;
    return true;
  }

  // Top-level implicit table (document root)
  c->nesting_level++;
  return true;
}

static bool toml_read_struct_key(void* ctx, const char** key) {
  toml_ctx_t* c = (toml_ctx_t*)ctx;
  text_parse_ctx_t* p = &c->parse;
  toml_skip_whitespace_and_comments(p);

  // Check if we've reached the end
  char ch = text_parse_peek_raw(p);

  if (c->in_inline_table) {
    // Skip comma between items
    if (!c->inline_first_item) {
      text_parse_match(p, ',');
      toml_skip_whitespace_and_comments(p);
      ch = text_parse_peek_raw(p);
    }
    c->inline_first_item = false;

    if (ch == '}') {
      return false;  // End of inline table
    }
  } else {
    if (text_parse_eof(p) || ch == '[') {
      return false;  // End of table or start of new section
    }
  }

  // Parse the key
  const char* k = toml_parse_key(p);
  if (!k) return false;

  // Skip equals sign
  toml_skip_whitespace_and_comments(p);
  if (!text_parse_match(p, '=')) return false;

  *key = k;
  return true;
}

static bool toml_read_struct_end(void* ctx) {
  toml_ctx_t* c = (toml_ctx_t*)ctx;
  text_parse_ctx_t* p = &c->parse;
  c->nesting_level--;

  if (c->in_inline_table) {
    toml_skip_whitespace_and_comments(p);
    text_parse_match(p, ',');  // Optional trailing comma
    c->in_inline_table = false;
    return text_parse_match(p, '}');
  }

  return true;
}

static bool toml_read_matrix(void* ctx, size_t* ndims, size_t** dims, double** data) {
  toml_ctx_t* c = (toml_ctx_t*)ctx;
  text_parse_ctx_t* p = &c->parse;
  toml_skip_whitespace_and_comments(p);

  // Expect inline table format: { dims = [...], data = [...] }
  if (!text_parse_match(p, '{')) return false;

  size_t* parsed_dims = NULL;
  double* parsed_data = NULL;
  size_t dim_count = 0;
  size_t data_count = 0;
  bool got_dims = false;
  bool got_data = false;

  while (true) {
    toml_skip_whitespace_and_comments(p);
    char ch = text_parse_peek_raw(p);
    if (ch == '}') break;

    // Skip comma
    text_parse_match(p, ',');
    toml_skip_whitespace_and_comments(p);
    if (text_parse_peek_raw(p) == '}') break;

    // Parse key
    const char* key = toml_parse_key(p);
    if (!key) goto error;

    toml_skip_whitespace_and_comments(p);
    if (!text_parse_match(p, '=')) goto error;
    toml_skip_whitespace_and_comments(p);

    if (strcmp(key, "dims") == 0) {
      // Parse dims list
      if (!text_parse_match(p, '[')) goto error;

      size_t dim_cap = 4;
      parsed_dims = olib_malloc(dim_cap * sizeof(size_t));
      if (!parsed_dims) goto error;

      while (text_parse_peek(p) != ']') {
        if (dim_count > 0) text_parse_match(p, ',');

        if (dim_count >= dim_cap) {
          dim_cap *= 2;
          size_t* new_dims = olib_realloc(parsed_dims, dim_cap * sizeof(size_t));
          if (!new_dims) goto error;
          parsed_dims = new_dims;
        }

        text_parse_number_result_t result;
        if (!text_parse_number(p, &result)) goto error;
        parsed_dims[dim_count++] = (size_t)result.int_value;
      }

      if (!text_parse_match(p, ']')) goto error;
      got_dims = true;

    } else if (strcmp(key, "data") == 0) {
      // Parse data list
      if (!text_parse_match(p, '[')) goto error;

      size_t data_cap = 64;
      parsed_data = olib_malloc(data_cap * sizeof(double));
      if (!parsed_data) goto error;

      while (text_parse_peek(p) != ']') {
        if (data_count > 0) text_parse_match(p, ',');

        if (data_count >= data_cap) {
          data_cap *= 2;
          double* new_data = olib_realloc(parsed_data, data_cap * sizeof(double));
          if (!new_data) goto error;
          parsed_data = new_data;
        }

        text_parse_number_result_t result;
        if (!text_parse_number(p, &result)) goto error;
        parsed_data[data_count++] = result.is_float ? result.float_value : (double)result.int_value;
      }

      if (!text_parse_match(p, ']')) goto error;
      got_data = true;
    }
  }

  if (!text_parse_match(p, '}')) goto error;

  if (!got_dims || !got_data) goto error;

  // Verify data size matches dimensions
  size_t expected_size = 1;
  for (size_t i = 0; i < dim_count; i++) {
    expected_size *= parsed_dims[i];
  }
  if (data_count != expected_size) goto error;

  *ndims = dim_count;
  *dims = parsed_dims;
  *data = parsed_data;
  return true;

error:
  if (parsed_dims) olib_free(parsed_dims);
  if (parsed_data) olib_free(parsed_data);
  return false;
}

// #############################################################################
// Lifecycle callbacks
// #############################################################################

static void toml_free_ctx(void* ctx) {
  toml_ctx_t* c = (toml_ctx_t*)ctx;
  if (c->write_buffer) olib_free(c->write_buffer);
  text_parse_free(&c->parse);
  olib_free(c);
}

static bool toml_init_write(void* ctx) {
  toml_ctx_t* c = (toml_ctx_t*)ctx;
  c->write_size = 0;
  c->nesting_level = 0;
  c->in_list = false;
  c->list_first_item = true;
  c->in_inline_table = false;
  c->inline_first_item = true;
  c->pending_key = NULL;
  return true;
}

static bool toml_finish_write(void* ctx, uint8_t** out_data, size_t* out_size) {
  toml_ctx_t* c = (toml_ctx_t*)ctx;
  if (!out_data || !out_size) {
    return false;
  }

  // Add null terminator
  if (!toml_ensure_write_capacity(c, 1)) return false;
  c->write_buffer[c->write_size] = '\0';

  *out_data = (uint8_t*)c->write_buffer;
  *out_size = c->write_size;

  c->write_buffer = NULL;
  c->write_capacity = 0;
  c->write_size = 0;
  return true;
}

static bool toml_init_read(void* ctx, const uint8_t* data, size_t size) {
  toml_ctx_t* c = (toml_ctx_t*)ctx;
  text_parse_init(&c->parse, (const char*)data, size);
  c->nesting_level = 0;
  c->in_list = false;
  c->in_inline_table = false;
  return true;
}

static bool toml_finish_read(void* ctx) {
  toml_ctx_t* c = (toml_ctx_t*)ctx;
  text_parse_reset(&c->parse);
  return true;
}

// #############################################################################
// Public API
// #############################################################################

OLIB_API olib_serializer_t* olib_serializer_new_toml() {
  toml_ctx_t* ctx = olib_calloc(1, sizeof(toml_ctx_t));
  if (!ctx) {
    return NULL;
  }

  olib_serializer_config_t config = {
    .user_data = ctx,
    .text_based = true,
    .free_ctx = toml_free_ctx,
    .init_write = toml_init_write,
    .finish_write = toml_finish_write,
    .init_read = toml_init_read,
    .finish_read = toml_finish_read,

    .write_int = toml_write_int,
    .write_uint = toml_write_uint,
    .write_float = toml_write_float,
    .write_string = toml_write_string,
    .write_bool = toml_write_bool,
    .write_list_begin = toml_write_list_begin,
    .write_list_end = toml_write_list_end,
    .write_struct_begin = toml_write_struct_begin,
    .write_struct_key = toml_write_struct_key,
    .write_struct_end = toml_write_struct_end,
    .write_matrix = toml_write_matrix,

    .read_peek = toml_read_peek,
    .read_int = toml_read_int,
    .read_uint = toml_read_uint,
    .read_float = toml_read_float,
    .read_string = toml_read_string,
    .read_bool = toml_read_bool,
    .read_list_begin = toml_read_list_begin,
    .read_list_end = toml_read_list_end,
    .read_struct_begin = toml_read_struct_begin,
    .read_struct_key = toml_read_struct_key,
    .read_struct_end = toml_read_struct_end,
    .read_matrix = toml_read_matrix,
  };

  olib_serializer_t* serializer = olib_serializer_new(&config);
  if (!serializer) {
    olib_free(ctx);
    return NULL;
  }

  return serializer;
}
