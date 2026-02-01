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

#include "text_parsing_utilities.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

// #############################################################################
// Context management
// #############################################################################

void text_parse_init(text_parse_ctx_t* ctx, const char* buffer, size_t size) {
  ctx->buffer = buffer;
  ctx->size = size;
  ctx->pos = 0;
  ctx->temp_string = NULL;
  ctx->temp_string_capacity = 0;
}

void text_parse_reset(text_parse_ctx_t* ctx) {
  ctx->buffer = NULL;
  ctx->size = 0;
  ctx->pos = 0;
}

void text_parse_free(text_parse_ctx_t* ctx) {
  if (ctx->temp_string) {
    olib_free(ctx->temp_string);
    ctx->temp_string = NULL;
    ctx->temp_string_capacity = 0;
  }
}

bool text_parse_ensure_temp(text_parse_ctx_t* ctx, size_t len) {
  size_t needed = len + 1;
  if (needed <= ctx->temp_string_capacity) {
    return true;
  }

  size_t new_cap = ctx->temp_string_capacity ? ctx->temp_string_capacity * 2 : 64;
  while (new_cap < needed) {
    new_cap *= 2;
  }

  char* new_buf = olib_realloc(ctx->temp_string, new_cap);
  if (!new_buf) return false;

  ctx->temp_string = new_buf;
  ctx->temp_string_capacity = new_cap;
  return true;
}

// #############################################################################
// Basic parsing operations
// #############################################################################

void text_parse_skip_whitespace(text_parse_ctx_t* ctx) {
  while (ctx->pos < ctx->size) {
    char c = ctx->buffer[ctx->pos];
    if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
      ctx->pos++;
    } else {
      break;
    }
  }
}

void text_parse_skip_whitespace_and_comments(text_parse_ctx_t* ctx) {
  while (ctx->pos < ctx->size) {
    char c = ctx->buffer[ctx->pos];
    if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
      ctx->pos++;
    } else if (c == '#') {
      // Skip to end of line
      while (ctx->pos < ctx->size && ctx->buffer[ctx->pos] != '\n') {
        ctx->pos++;
      }
    } else {
      break;
    }
  }
}

char text_parse_peek(text_parse_ctx_t* ctx) {
  text_parse_skip_whitespace(ctx);
  if (ctx->pos >= ctx->size) return '\0';
  return ctx->buffer[ctx->pos];
}

char text_parse_peek_skip_comments(text_parse_ctx_t* ctx) {
  text_parse_skip_whitespace_and_comments(ctx);
  if (ctx->pos >= ctx->size) return '\0';
  return ctx->buffer[ctx->pos];
}

char text_parse_peek_raw(text_parse_ctx_t* ctx) {
  if (ctx->pos >= ctx->size) return '\0';
  return ctx->buffer[ctx->pos];
}

bool text_parse_eof(text_parse_ctx_t* ctx) {
  return ctx->pos >= ctx->size;
}

char text_parse_consume(text_parse_ctx_t* ctx) {
  if (ctx->pos >= ctx->size) return '\0';
  return ctx->buffer[ctx->pos++];
}

bool text_parse_match(text_parse_ctx_t* ctx, char expected) {
  text_parse_skip_whitespace(ctx);
  if (ctx->pos >= ctx->size) return false;
  if (ctx->buffer[ctx->pos] != expected) return false;
  ctx->pos++;
  return true;
}

bool text_parse_match_raw(text_parse_ctx_t* ctx, char expected) {
  if (ctx->pos >= ctx->size) return false;
  if (ctx->buffer[ctx->pos] != expected) return false;
  ctx->pos++;
  return true;
}

bool text_parse_match_str(text_parse_ctx_t* ctx, const char* expected) {
  text_parse_skip_whitespace(ctx);
  size_t len = strlen(expected);
  if (ctx->pos + len > ctx->size) return false;
  if (strncmp(ctx->buffer + ctx->pos, expected, len) != 0) return false;
  ctx->pos += len;
  return true;
}

// #############################################################################
// Token parsing
// #############################################################################

bool text_parse_is_identifier_char(char c) {
  return (c >= 'a' && c <= 'z') ||
         (c >= 'A' && c <= 'Z') ||
         (c >= '0' && c <= '9') ||
         c == '_';
}

bool text_parse_is_identifier_start(char c) {
  return (c >= 'a' && c <= 'z') ||
         (c >= 'A' && c <= 'Z') ||
         c == '_';
}

const char* text_parse_identifier(text_parse_ctx_t* ctx) {
  text_parse_skip_whitespace(ctx);
  size_t start = ctx->pos;

  // First char must be identifier start
  if (ctx->pos >= ctx->size || !text_parse_is_identifier_char(ctx->buffer[ctx->pos])) {
    return NULL;
  }

  while (ctx->pos < ctx->size && text_parse_is_identifier_char(ctx->buffer[ctx->pos])) {
    ctx->pos++;
  }

  size_t len = ctx->pos - start;
  if (len == 0) return NULL;

  if (!text_parse_ensure_temp(ctx, len)) return NULL;
  memcpy(ctx->temp_string, ctx->buffer + start, len);
  ctx->temp_string[len] = '\0';
  return ctx->temp_string;
}

// #############################################################################
// Number parsing
// #############################################################################

bool text_parse_number(text_parse_ctx_t* ctx, text_parse_number_result_t* result) {
  text_parse_skip_whitespace(ctx);
  size_t start = ctx->pos;
  result->is_float = false;
  result->is_negative = false;

  // Handle sign
  if (ctx->pos < ctx->size) {
    if (ctx->buffer[ctx->pos] == '-') {
      result->is_negative = true;
      ctx->pos++;
    } else if (ctx->buffer[ctx->pos] == '+') {
      ctx->pos++;
    }
  }

  // Must have at least one digit
  if (ctx->pos >= ctx->size || !isdigit((unsigned char)ctx->buffer[ctx->pos])) {
    ctx->pos = start;
    return false;
  }

  // Integer part
  while (ctx->pos < ctx->size && isdigit((unsigned char)ctx->buffer[ctx->pos])) {
    ctx->pos++;
  }

  // Decimal part
  if (ctx->pos < ctx->size && ctx->buffer[ctx->pos] == '.') {
    result->is_float = true;
    ctx->pos++;
    while (ctx->pos < ctx->size && isdigit((unsigned char)ctx->buffer[ctx->pos])) {
      ctx->pos++;
    }
  }

  // Exponent part
  if (ctx->pos < ctx->size && (ctx->buffer[ctx->pos] == 'e' || ctx->buffer[ctx->pos] == 'E')) {
    result->is_float = true;
    ctx->pos++;
    if (ctx->pos < ctx->size && (ctx->buffer[ctx->pos] == '-' || ctx->buffer[ctx->pos] == '+')) {
      ctx->pos++;
    }
    while (ctx->pos < ctx->size && isdigit((unsigned char)ctx->buffer[ctx->pos])) {
      ctx->pos++;
    }
  }

  size_t len = ctx->pos - start;
  if (len == 0) {
    ctx->pos = start;
    return false;
  }

  if (!text_parse_ensure_temp(ctx, len)) {
    ctx->pos = start;
    return false;
  }
  memcpy(ctx->temp_string, ctx->buffer + start, len);
  ctx->temp_string[len] = '\0';

  if (result->is_float) {
    result->float_value = strtod(ctx->temp_string, NULL);
    result->int_value = (int64_t)result->float_value;
  } else {
    result->int_value = strtoll(ctx->temp_string, NULL, 10);
    result->float_value = (double)result->int_value;
  }
  return true;
}

// #############################################################################
// String parsing
// #############################################################################

const char* text_parse_quoted_string(text_parse_ctx_t* ctx) {
  text_parse_skip_whitespace(ctx);

  if (ctx->pos >= ctx->size || ctx->buffer[ctx->pos] != '"') {
    return NULL;
  }
  ctx->pos++; // Skip opening quote

  // First pass: count length (handling escapes)
  size_t scan_pos = ctx->pos;
  size_t len = 0;
  while (scan_pos < ctx->size && ctx->buffer[scan_pos] != '"') {
    if (ctx->buffer[scan_pos] == '\\' && scan_pos + 1 < ctx->size) {
      scan_pos += 2;
      len++;
    } else {
      scan_pos++;
      len++;
    }
  }

  if (!text_parse_ensure_temp(ctx, len)) return NULL;

  // Second pass: copy with escape handling
  size_t out_pos = 0;
  while (ctx->pos < ctx->size && ctx->buffer[ctx->pos] != '"') {
    if (ctx->buffer[ctx->pos] == '\\' && ctx->pos + 1 < ctx->size) {
      ctx->pos++;
      switch (ctx->buffer[ctx->pos]) {
        case 'n':  ctx->temp_string[out_pos++] = '\n'; break;
        case 'r':  ctx->temp_string[out_pos++] = '\r'; break;
        case 't':  ctx->temp_string[out_pos++] = '\t'; break;
        case '"':  ctx->temp_string[out_pos++] = '"';  break;
        case '\\': ctx->temp_string[out_pos++] = '\\'; break;
        case '/':  ctx->temp_string[out_pos++] = '/';  break;
        case 'b':  ctx->temp_string[out_pos++] = '\b'; break;
        case 'f':  ctx->temp_string[out_pos++] = '\f'; break;
        default:   ctx->temp_string[out_pos++] = ctx->buffer[ctx->pos]; break;
      }
      ctx->pos++;
    } else {
      ctx->temp_string[out_pos++] = ctx->buffer[ctx->pos++];
    }
  }
  ctx->temp_string[out_pos] = '\0';

  if (ctx->pos >= ctx->size || ctx->buffer[ctx->pos] != '"') {
    return NULL; // Unterminated string
  }
  ctx->pos++; // Skip closing quote

  return ctx->temp_string;
}

const char* text_parse_single_quoted_string(text_parse_ctx_t* ctx) {
  text_parse_skip_whitespace(ctx);

  if (ctx->pos >= ctx->size || ctx->buffer[ctx->pos] != '\'') {
    return NULL;
  }
  ctx->pos++; // Skip opening quote

  // First pass: count length
  size_t scan_pos = ctx->pos;
  size_t len = 0;
  while (scan_pos < ctx->size && ctx->buffer[scan_pos] != '\'') {
    if (ctx->buffer[scan_pos] == '\\' && scan_pos + 1 < ctx->size &&
        (ctx->buffer[scan_pos + 1] == '\'' || ctx->buffer[scan_pos + 1] == '\\')) {
      scan_pos += 2;
      len++;
    } else {
      scan_pos++;
      len++;
    }
  }

  if (!text_parse_ensure_temp(ctx, len)) return NULL;

  // Second pass: copy
  size_t out_pos = 0;
  while (ctx->pos < ctx->size && ctx->buffer[ctx->pos] != '\'') {
    if (ctx->buffer[ctx->pos] == '\\' && ctx->pos + 1 < ctx->size) {
      char next = ctx->buffer[ctx->pos + 1];
      if (next == '\'' || next == '\\') {
        ctx->pos++;
        ctx->temp_string[out_pos++] = ctx->buffer[ctx->pos++];
      } else {
        ctx->temp_string[out_pos++] = ctx->buffer[ctx->pos++];
      }
    } else {
      ctx->temp_string[out_pos++] = ctx->buffer[ctx->pos++];
    }
  }
  ctx->temp_string[out_pos] = '\0';

  if (ctx->pos >= ctx->size || ctx->buffer[ctx->pos] != '\'') {
    return NULL; // Unterminated string
  }
  ctx->pos++; // Skip closing quote

  return ctx->temp_string;
}

// #############################################################################
// Utility functions
// #############################################################################

size_t text_parse_line_number(text_parse_ctx_t* ctx) {
  size_t line = 1;
  for (size_t i = 0; i < ctx->pos && i < ctx->size; i++) {
    if (ctx->buffer[i] == '\n') {
      line++;
    }
  }
  return line;
}

size_t text_parse_column_number(text_parse_ctx_t* ctx) {
  size_t col = 1;
  for (size_t i = 0; i < ctx->pos && i < ctx->size; i++) {
    if (ctx->buffer[i] == '\n') {
      col = 1;
    } else {
      col++;
    }
  }
  return col;
}
