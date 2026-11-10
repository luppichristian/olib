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
#include <float.h>
#include <errno.h>
#include <math.h>

// #############################################################################
// Context structure for TOML serialization
// #############################################################################

typedef enum { TOML_ROOT, TOML_ARRAY, TOML_INLINE_TABLE } toml_container_t;

typedef struct {
  toml_container_t type;
  bool first;
} toml_frame_t;

#define TOML_MAX_DEPTH 128

typedef struct {
  // Write mode
  char* write_buffer;
  size_t write_capacity;
  size_t write_size;
  toml_frame_t stack[TOML_MAX_DEPTH];
  size_t depth;
  const char* pending_key;

  // Read mode (using shared parsing utilities)
  text_parse_ctx_t parse;
  bool read_error;
} toml_ctx_t;

static toml_frame_t* toml_top(toml_ctx_t* ctx) {
  return ctx->depth ? &ctx->stack[ctx->depth - 1] : NULL;
}

static bool toml_push(toml_ctx_t* ctx, toml_container_t type) {
  if (ctx->depth == TOML_MAX_DEPTH) return false;
  ctx->stack[ctx->depth].type = type;
  ctx->stack[ctx->depth].first = true;
  ctx->depth++;
  return true;
}

static bool toml_is_root_member(toml_ctx_t* ctx) {
  toml_frame_t* frame = toml_top(ctx);
  return frame && frame->type == TOML_ROOT;
}

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
        if ((unsigned char)*p < 0x20 || (unsigned char)*p == 0x7f) {
          char escape[7];
          snprintf(escape, sizeof(escape), "\\u%04x", (unsigned char)*p);
          if (!toml_write_str(ctx, escape)) return false;
        } else if (!toml_write_char(ctx, *p)) return false;
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
  toml_frame_t* frame = toml_top(ctx);
  if (!frame || frame->type == TOML_ROOT) return true;
  if (!frame->first && !toml_write_str(ctx, ", ")) return false;
  frame->first = false;
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
  if (toml_is_root_member(c)) {
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
  if (toml_is_root_member(c)) {
    if (!toml_write_char(c, '\n')) return false;
  }

  return true;
}

static bool toml_write_float(void* ctx, double value) {
  toml_ctx_t* c = (toml_ctx_t*)ctx;

  // Until the reader supports TOML's inf/nan tokens, reject them instead of
  // emitting an unparseable value or silently changing the input.
  if (!isfinite(value)) return false;

  if (!toml_write_item_separator(c)) return false;
  if (!toml_write_key_prefix(c)) return false;

  char buf[64];
  // Preserve every finite double while retaining TOML's explicit float syntax.
  snprintf(buf, sizeof(buf), "%.*g", DBL_DECIMAL_DIG, value);
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
  if (toml_is_root_member(c)) {
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
          if ((unsigned char)*p < 0x20 || (unsigned char)*p == 0x7f) {
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
  if (toml_is_root_member(c)) {
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
  if (toml_is_root_member(c)) {
    if (!toml_write_char(c, '\n')) return false;
  }

  return true;
}

static bool toml_write_list_begin(void* ctx, size_t size) {
  (void)size;
  toml_ctx_t* c = (toml_ctx_t*)ctx;
  if (c->depth == TOML_MAX_DEPTH) return false;
  if (!toml_write_item_separator(c)) return false;
  if (!toml_write_key_prefix(c)) return false;
  if (!toml_write_char(c, '[')) return false;
  return toml_push(c, TOML_ARRAY);
}

static bool toml_write_list_end(void* ctx) {
  toml_ctx_t* c = (toml_ctx_t*)ctx;
  if (!toml_top(c) || toml_top(c)->type != TOML_ARRAY) return false;
  if (!toml_write_char(c, ']')) return false;
  c->depth--;
  if (toml_is_root_member(c)) {
    if (!toml_write_char(c, '\n')) return false;
  }

  return true;
}

static bool toml_write_struct_begin(void* ctx) {
  toml_ctx_t* c = (toml_ctx_t*)ctx;
  if (c->depth == TOML_MAX_DEPTH) return false;
  if (c->depth == 0) return toml_push(c, TOML_ROOT);
  if (!toml_write_item_separator(c)) return false;
  if (!toml_write_key_prefix(c)) return false;
  if (!toml_write_char(c, '{')) return false;
  return toml_push(c, TOML_INLINE_TABLE);
}

static bool toml_write_struct_key(void* ctx, const char* key) {
  toml_ctx_t* c = (toml_ctx_t*)ctx;
  c->pending_key = key;
  return true;
}

static bool toml_write_struct_end(void* ctx) {
  toml_ctx_t* c = (toml_ctx_t*)ctx;
  toml_frame_t* frame = toml_top(c);
  if (!frame || frame->type == TOML_ARRAY) return false;
  const bool is_inline = frame->type == TOML_INLINE_TABLE;
  if (is_inline) {
    if (!toml_write_char(c, '}')) return false;
  }
  c->depth--;
  if (is_inline && toml_is_root_member(c) && !toml_write_char(c, '\n')) return false;
  return true;
}

// #############################################################################
// Read helpers
// #############################################################################

// Skip TOML comments (# to end of line)
static void toml_skip_whitespace_and_comments(text_parse_ctx_t* p) {
  text_parse_skip_whitespace_and_comments(p);
}

static int toml_hex_digit(char ch) {
  if (ch >= '0' && ch <= '9') return ch - '0';
  if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
  if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
  return -1;
}

// Decode TOML basic strings into the context's reusable temporary buffer.
// The encoded length is an upper bound for the UTF-8 decoded length.
static const char* toml_parse_basic_string(text_parse_ctx_t* p) {
  text_parse_skip_whitespace(p);
  if (p->pos >= p->size || p->buffer[p->pos] != '"') return NULL;
  size_t start = ++p->pos;
  size_t end = start;
  while (end < p->size && p->buffer[end] != '"') {
    if (p->buffer[end] == '\\' && end + 1 < p->size) ++end;
    ++end;
  }
  if (end >= p->size || !text_parse_ensure_temp(p, end - start)) return NULL;

  size_t out = 0;
  while (p->pos < end) {
    unsigned char ch = (unsigned char)p->buffer[p->pos++];
    if (ch != '\\') {
      if ((ch < 0x20 && ch != '\t') || ch == 0x7f) return NULL;
      p->temp_string[out++] = (char)ch;
      continue;
    }
    if (p->pos >= end) return NULL;
    char esc = p->buffer[p->pos++];
    switch (esc) {
      case 'b': p->temp_string[out++] = '\b'; break;
      case 't': p->temp_string[out++] = '\t'; break;
      case 'n': p->temp_string[out++] = '\n'; break;
      case 'f': p->temp_string[out++] = '\f'; break;
      case 'r': p->temp_string[out++] = '\r'; break;
      case '"': p->temp_string[out++] = '"'; break;
      case '\\': p->temp_string[out++] = '\\'; break;
      case 'u':
      case 'U': {
        size_t digits = esc == 'u' ? 4 : 8;
        if (end - p->pos < digits) return NULL;
        uint32_t code = 0;
        for (size_t i = 0; i < digits; ++i) {
          int digit = toml_hex_digit(p->buffer[p->pos++]);
          if (digit < 0) return NULL;
          code = (code << 4) | (uint32_t)digit;
        }
        // The object API stores NUL-terminated strings and cannot represent U+0000.
        if (!code || code > 0x10ffff || (code >= 0xd800 && code <= 0xdfff)) return NULL;
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
        break;
      }
      default: return NULL;
    }
  }
  p->temp_string[out] = '\0';
  ++p->pos;
  return p->temp_string;
}

// Parse a TOML key (bare key or quoted key)
static const char* toml_parse_key(text_parse_ctx_t* p) {
  text_parse_skip_whitespace(p);

  char ch = text_parse_peek_raw(p);

  // Quoted key (basic string)
  if (ch == '"') {
    return toml_parse_basic_string(p);
  }

  // Quoted key (literal string)
  if (ch == '\'') {
    return text_parse_single_quoted_string(p);
  }

  // Bare TOML keys may contain letters, digits, underscores, and hyphens.
  size_t start = p->pos;
  while (p->pos < p->size) {
    ch = p->buffer[p->pos];
    if (!((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
          (ch >= '0' && ch <= '9') || ch == '_' || ch == '-')) break;
    ++p->pos;
  }
  size_t len = p->pos - start;
  if (!len || !text_parse_ensure_temp(p, len)) return NULL;
  memcpy(p->temp_string, p->buffer + start, len);
  p->temp_string[len] = '\0';
  return p->temp_string;
}

// Parse a TOML literal string (single quotes, no escapes except '' for ')
static const char* toml_parse_literal_string(text_parse_ctx_t* p) {
  return text_parse_single_quoted_string(p);
}

// #############################################################################
// Read callbacks
// #############################################################################

// Recognize a root key without allocating; peeking must not turn an allocation
// failure into a successful parse of the key itself as a scalar value.
static bool toml_looks_like_root_key(const text_parse_ctx_t* p) {
  size_t pos = p->pos;
  if (pos >= p->size) return false;
  char quote = p->buffer[pos];
  if (quote == '"' || quote == '\'') {
    ++pos;
    bool closed = false;
    while (pos < p->size) {
      char ch = p->buffer[pos++];
      if (quote == '"' && ch == '\\' && pos < p->size) { ++pos; continue; }
      if (ch == quote) { closed = true; break; }
      if (ch == '\n' || ch == '\r') return false;
    }
    if (!closed) return false;
  } else {
    size_t start = pos;
    while (pos < p->size) {
      char ch = p->buffer[pos];
      if (!((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
            (ch >= '0' && ch <= '9') || ch == '_' || ch == '-')) break;
      ++pos;
    }
    if (pos == start) return false;
  }
  while (pos < p->size && (p->buffer[pos] == ' ' || p->buffer[pos] == '\t')) ++pos;
  return pos < p->size && p->buffer[pos] == '=';
}

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

  // A document root begins with a key, which may itself look like a scalar
  // (e.g. a quoted key or a bare numeric key).
  if (c->depth == 0 && toml_looks_like_root_key(p)) return OLIB_OBJECT_TYPE_STRUCT;

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
    if (ch != '-') {
      size_t first = p->pos + (ch == '+' ? 1 : 0);
      while (first < pos && p->buffer[first] == '0') ++first;
      size_t digits = pos - first;
      static const char signed_max[] = "9223372036854775807";
      if (digits > sizeof(signed_max) - 1 ||
          (digits == sizeof(signed_max) - 1 &&
           memcmp(p->buffer + first, signed_max, digits) > 0)) {
        return OLIB_OBJECT_TYPE_UINT;
      }
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

  return OLIB_OBJECT_TYPE_MAX;
}

static bool toml_read_int(void* ctx, int64_t* value) {
  toml_ctx_t* c = (toml_ctx_t*)ctx;
  text_parse_number_result_t result;
  errno = 0;
  if (!text_parse_number(&c->parse, &result)) return false;
  if (errno == ERANGE || (!result.is_negative && result.uint_value > INT64_MAX)) return false;
  *value = result.int_value;
  return true;
}

static bool toml_read_uint(void* ctx, uint64_t* value) {
  toml_ctx_t* c = (toml_ctx_t*)ctx;
  text_parse_number_result_t result;
  if (!text_parse_number(&c->parse, &result)) return false;
  *value = result.uint_value;
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
    const char* str = toml_parse_basic_string(p);
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

  if (c->depth == TOML_MAX_DEPTH) return false;
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
  return toml_push(c, TOML_ARRAY);
}

static bool toml_read_list_end(void* ctx) {
  toml_ctx_t* c = (toml_ctx_t*)ctx;
  text_parse_ctx_t* p = &c->parse;
  if (!toml_top(c) || toml_top(c)->type != TOML_ARRAY) return false;
  toml_skip_whitespace_and_comments(p);
  text_parse_match(p, ',');  // Optional trailing comma
  if (!text_parse_match(p, ']')) return false;
  c->depth--;
  return true;
}

static bool toml_read_struct_begin(void* ctx) {
  toml_ctx_t* c = (toml_ctx_t*)ctx;
  text_parse_ctx_t* p = &c->parse;
  if (c->depth == TOML_MAX_DEPTH) return false;
  toml_skip_whitespace_and_comments(p);

  // Check for inline table
  if (text_parse_peek_raw(p) == '{') {
    text_parse_match(p, '{');
    return toml_push(c, TOML_INLINE_TABLE);
  }

  // Top-level implicit table (document root)
  if (c->depth != 0) return false;
  return toml_push(c, TOML_ROOT);
}

static bool toml_read_struct_key(void* ctx, const char** key) {
  toml_ctx_t* c = (toml_ctx_t*)ctx;
  text_parse_ctx_t* p = &c->parse;
  toml_skip_whitespace_and_comments(p);

  // Check if we've reached the end
  char ch = text_parse_peek_raw(p);

  toml_frame_t* frame = toml_top(c);
  if (!frame || frame->type == TOML_ARRAY) return false;
  if (frame->type == TOML_INLINE_TABLE) {
    // Skip comma between items
    if (!frame->first) {
      text_parse_match(p, ',');
      toml_skip_whitespace_and_comments(p);
      ch = text_parse_peek_raw(p);
    }
    frame->first = false;

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
  if (!k) {
    c->read_error = true;
    return false;
  }

  // Skip equals sign
  toml_skip_whitespace_and_comments(p);
  if (!text_parse_match(p, '=')) {
    c->read_error = true;
    return false;
  }

  *key = k;
  return true;
}

static bool toml_read_struct_end(void* ctx) {
  toml_ctx_t* c = (toml_ctx_t*)ctx;
  text_parse_ctx_t* p = &c->parse;
  if (c->read_error) return false;
  toml_frame_t* frame = toml_top(c);
  if (!frame || frame->type == TOML_ARRAY) return false;
  if (frame->type == TOML_INLINE_TABLE) {
    toml_skip_whitespace_and_comments(p);
    text_parse_match(p, ',');  // Optional trailing comma
    if (!text_parse_match(p, '}')) return false;
  }
  c->depth--;
  return true;
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
  c->depth = 0;
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
  c->depth = 0;
  c->read_error = false;
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
  };

  olib_serializer_t* serializer = olib_serializer_new(&config);
  if (!serializer) {
    olib_free(ctx);
    return NULL;
  }

  return serializer;
}
