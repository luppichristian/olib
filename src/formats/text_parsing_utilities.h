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

#pragma once

#include <olib.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

// #############################################################################
// Text parsing context
// #############################################################################

typedef struct {
  const char* buffer;
  size_t size;
  size_t pos;
  char* temp_string;
  size_t temp_string_capacity;
} text_parse_ctx_t;

// #############################################################################
// Context management
// #############################################################################

// Initialize a parsing context with the given buffer
void text_parse_init(text_parse_ctx_t* ctx, const char* buffer, size_t size);

// Reset the parsing context (keeps temp_string allocation)
void text_parse_reset(text_parse_ctx_t* ctx);

// Free resources associated with parsing context
void text_parse_free(text_parse_ctx_t* ctx);

// Ensure temp_string can hold at least 'len' characters (plus null terminator)
bool text_parse_ensure_temp(text_parse_ctx_t* ctx, size_t len);

// #############################################################################
// Basic parsing operations
// #############################################################################

// Skip whitespace characters (space, tab, newline, carriage return)
void text_parse_skip_whitespace(text_parse_ctx_t* ctx);

// Skip whitespace and # comments (to end of line)
void text_parse_skip_whitespace_and_comments(text_parse_ctx_t* ctx);

// Peek at the next non-whitespace character without consuming it
char text_parse_peek(text_parse_ctx_t* ctx);

// Peek at the next non-whitespace character, also skipping comments
char text_parse_peek_skip_comments(text_parse_ctx_t* ctx);

// Peek at the character at current position without skipping whitespace
char text_parse_peek_raw(text_parse_ctx_t* ctx);

// Check if at end of input
bool text_parse_eof(text_parse_ctx_t* ctx);

// Consume and return the next character (no whitespace skip)
char text_parse_consume(text_parse_ctx_t* ctx);

// Match expected character (skips whitespace first), returns true if matched and consumed
bool text_parse_match(text_parse_ctx_t* ctx, char expected);

// Match expected character without skipping whitespace
bool text_parse_match_raw(text_parse_ctx_t* ctx, char expected);

// Match expected string (skips whitespace first), returns true if matched and consumed
bool text_parse_match_str(text_parse_ctx_t* ctx, const char* expected);

// #############################################################################
// Token parsing
// #############################################################################

// Check if character is valid in an identifier (a-z, A-Z, 0-9, _)
bool text_parse_is_identifier_char(char c);

// Check if character is valid as first character of identifier (a-z, A-Z, _)
bool text_parse_is_identifier_start(char c);

// Read an identifier into temp_string, returns pointer to temp_string or NULL on failure
const char* text_parse_identifier(text_parse_ctx_t* ctx);

// #############################################################################
// Number parsing
// #############################################################################

typedef struct {
  double float_value;
  int64_t int_value;
  uint64_t uint_value;
  bool is_float;
  bool is_negative;
} text_parse_number_result_t;

// Parse a number (integer or floating point)
// Returns true on success, result contains the parsed value
bool text_parse_number(text_parse_ctx_t* ctx, text_parse_number_result_t* result);

// #############################################################################
// String parsing
// #############################################################################

// Parse a double-quoted string with escape sequences into temp_string
// Handles: \n, \r, \t, \\, \"
// Returns pointer to temp_string or NULL on failure
const char* text_parse_quoted_string(text_parse_ctx_t* ctx);

// Parse a single-quoted string (no escape sequences except \' and \\)
// Returns pointer to temp_string or NULL on failure
const char* text_parse_single_quoted_string(text_parse_ctx_t* ctx);

// #############################################################################
// Utility functions
// #############################################################################

// Get current line number (1-based) for error reporting
size_t text_parse_line_number(text_parse_ctx_t* ctx);

// Get current column number (1-based) for error reporting
size_t text_parse_column_number(text_parse_ctx_t* ctx);
