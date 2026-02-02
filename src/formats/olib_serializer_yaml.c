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
// Context structure for YAML serialization
// #############################################################################

typedef struct {
  // Write mode
  char* write_buffer;
  size_t write_capacity;
  size_t write_size;
  int indent_level;
  bool in_flow_list;
  bool flow_list_first_item;
  bool in_block_list;
  bool block_list_needs_newline;
  bool in_struct;
  bool struct_first_item;
  bool struct_inline_value;
  const char* pending_key;

  // Read mode (using shared parsing utilities)
  text_parse_ctx_t parse;
  int read_indent_level;
} yaml_ctx_t;

// #############################################################################
// Write helpers
// #############################################################################

static bool yaml_ensure_write_capacity(yaml_ctx_t* ctx, size_t needed) {
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

static bool yaml_write_str(yaml_ctx_t* ctx, const char* str) {
  size_t len = strlen(str);
  if (!yaml_ensure_write_capacity(ctx, len)) return false;
  memcpy(ctx->write_buffer + ctx->write_size, str, len);
  ctx->write_size += len;
  return true;
}

static bool yaml_write_char(yaml_ctx_t* ctx, char c) {
  if (!yaml_ensure_write_capacity(ctx, 1)) return false;
  ctx->write_buffer[ctx->write_size++] = c;
  return true;
}

static bool yaml_write_indent(yaml_ctx_t* ctx) {
  // YAML uses 2-space indentation
  for (int i = 0; i < ctx->indent_level; i++) {
    if (!yaml_write_str(ctx, "  ")) return false;
  }
  return true;
}

static bool yaml_write_newline_indent(yaml_ctx_t* ctx) {
  if (!yaml_write_char(ctx, '\n')) return false;
  return yaml_write_indent(ctx);
}

// Check if string needs quoting in YAML
static bool yaml_needs_quoting(const char* str) {
  if (!str || !*str) return true;  // Empty strings need quotes

  // Check first character
  char first = str[0];
  if (first == '-' || first == '?' || first == ':' || first == ',' ||
      first == '[' || first == ']' || first == '{' || first == '}' ||
      first == '#' || first == '&' || first == '*' || first == '!' ||
      first == '|' || first == '>' || first == '\'' || first == '"' ||
      first == '%' || first == '@' || first == '`' || first == ' ') {
    return true;
  }

  // Check for reserved words
  if (strcmp(str, "true") == 0 || strcmp(str, "false") == 0 ||
      strcmp(str, "True") == 0 || strcmp(str, "False") == 0 ||
      strcmp(str, "TRUE") == 0 || strcmp(str, "FALSE") == 0 ||
      strcmp(str, "null") == 0 || strcmp(str, "Null") == 0 ||
      strcmp(str, "NULL") == 0 || strcmp(str, "~") == 0 ||
      strcmp(str, "yes") == 0 || strcmp(str, "no") == 0 ||
      strcmp(str, "Yes") == 0 || strcmp(str, "No") == 0 ||
      strcmp(str, "YES") == 0 || strcmp(str, "NO") == 0 ||
      strcmp(str, "on") == 0 || strcmp(str, "off") == 0 ||
      strcmp(str, "On") == 0 || strcmp(str, "Off") == 0 ||
      strcmp(str, "ON") == 0 || strcmp(str, "OFF") == 0) {
    return true;
  }

  // Check if it looks like a number
  const char* p = str;
  if (*p == '-' || *p == '+') p++;
  if (isdigit((unsigned char)*p)) {
    return true;  // Might be parsed as number
  }

  // Check for special characters in the string
  for (p = str; *p; p++) {
    if (*p == ':' || *p == '#' || *p == '\n' || *p == '\r' ||
        *p == '\t' || *p == '\\' || *p == '"' || *p == '\'' ||
        *p == '[' || *p == ']' || *p == '{' || *p == '}' ||
        *p == ',' || *p == '&' || *p == '*' || *p == '!' ||
        *p == '|' || *p == '>' || *p == '%' || *p == '@') {
      return true;
    }
  }

  return false;
}

static bool yaml_write_key_prefix(yaml_ctx_t* ctx) {
  if (ctx->pending_key) {
    if (!yaml_write_str(ctx, ctx->pending_key)) return false;
    if (!yaml_write_str(ctx, ": ")) return false;
    ctx->pending_key = NULL;
    ctx->struct_inline_value = true;
  }
  return true;
}

// #############################################################################
// Write callbacks
// #############################################################################

static bool yaml_write_int(void* ctx, int64_t value) {
  yaml_ctx_t* c = (yaml_ctx_t*)ctx;

  if (c->in_flow_list) {
    if (!c->flow_list_first_item) {
      if (!yaml_write_str(c, ", ")) return false;
    }
    c->flow_list_first_item = false;
  } else if (c->in_block_list) {
    if (!yaml_write_newline_indent(c)) return false;
    if (!yaml_write_str(c, "- ")) return false;
  } else if (c->in_struct && !c->struct_inline_value) {
    if (!c->struct_first_item) {
      if (!yaml_write_char(c, '\n')) return false;
    }
    c->struct_first_item = false;
    if (!yaml_write_indent(c)) return false;
  }

  if (!yaml_write_key_prefix(c)) return false;
  c->struct_inline_value = false;

  char buf[32];
  snprintf(buf, sizeof(buf), "%lld", (long long)value);
  return yaml_write_str(c, buf);
}

static bool yaml_write_uint(void* ctx, uint64_t value) {
  yaml_ctx_t* c = (yaml_ctx_t*)ctx;

  if (c->in_flow_list) {
    if (!c->flow_list_first_item) {
      if (!yaml_write_str(c, ", ")) return false;
    }
    c->flow_list_first_item = false;
  } else if (c->in_block_list) {
    if (!yaml_write_newline_indent(c)) return false;
    if (!yaml_write_str(c, "- ")) return false;
  } else if (c->in_struct && !c->struct_inline_value) {
    if (!c->struct_first_item) {
      if (!yaml_write_char(c, '\n')) return false;
    }
    c->struct_first_item = false;
    if (!yaml_write_indent(c)) return false;
  }

  if (!yaml_write_key_prefix(c)) return false;
  c->struct_inline_value = false;

  char buf[32];
  snprintf(buf, sizeof(buf), "%llu", (unsigned long long)value);
  return yaml_write_str(c, buf);
}

static bool yaml_write_float(void* ctx, double value) {
  yaml_ctx_t* c = (yaml_ctx_t*)ctx;

  if (c->in_flow_list) {
    if (!c->flow_list_first_item) {
      if (!yaml_write_str(c, ", ")) return false;
    }
    c->flow_list_first_item = false;
  } else if (c->in_block_list) {
    if (!yaml_write_newline_indent(c)) return false;
    if (!yaml_write_str(c, "- ")) return false;
  } else if (c->in_struct && !c->struct_inline_value) {
    if (!c->struct_first_item) {
      if (!yaml_write_char(c, '\n')) return false;
    }
    c->struct_first_item = false;
    if (!yaml_write_indent(c)) return false;
  }

  if (!yaml_write_key_prefix(c)) return false;
  c->struct_inline_value = false;

  char buf[64];
  snprintf(buf, sizeof(buf), "%g", value);
  return yaml_write_str(c, buf);
}

static bool yaml_write_string(void* ctx, const char* value) {
  yaml_ctx_t* c = (yaml_ctx_t*)ctx;

  if (c->in_flow_list) {
    if (!c->flow_list_first_item) {
      if (!yaml_write_str(c, ", ")) return false;
    }
    c->flow_list_first_item = false;
  } else if (c->in_block_list) {
    if (!yaml_write_newline_indent(c)) return false;
    if (!yaml_write_str(c, "- ")) return false;
  } else if (c->in_struct && !c->struct_inline_value) {
    if (!c->struct_first_item) {
      if (!yaml_write_char(c, '\n')) return false;
    }
    c->struct_first_item = false;
    if (!yaml_write_indent(c)) return false;
  }

  if (!yaml_write_key_prefix(c)) return false;
  c->struct_inline_value = false;

  // Check if we need to quote the string
  if (!value || yaml_needs_quoting(value)) {
    if (!yaml_write_char(c, '"')) return false;
    if (value) {
      // Escape special characters
      for (const char* p = value; *p; p++) {
        switch (*p) {
          case '"':
            if (!yaml_write_str(c, "\\\"")) return false;
            break;
          case '\\':
            if (!yaml_write_str(c, "\\\\")) return false;
            break;
          case '\n':
            if (!yaml_write_str(c, "\\n")) return false;
            break;
          case '\r':
            if (!yaml_write_str(c, "\\r")) return false;
            break;
          case '\t':
            if (!yaml_write_str(c, "\\t")) return false;
            break;
          default:
            if (!yaml_write_char(c, *p)) return false;
            break;
        }
      }
    }
    if (!yaml_write_char(c, '"')) return false;
  } else {
    // Unquoted string
    if (!yaml_write_str(c, value)) return false;
  }
  return true;
}

static bool yaml_write_bool(void* ctx, bool value) {
  yaml_ctx_t* c = (yaml_ctx_t*)ctx;

  if (c->in_flow_list) {
    if (!c->flow_list_first_item) {
      if (!yaml_write_str(c, ", ")) return false;
    }
    c->flow_list_first_item = false;
  } else if (c->in_block_list) {
    if (!yaml_write_newline_indent(c)) return false;
    if (!yaml_write_str(c, "- ")) return false;
  } else if (c->in_struct && !c->struct_inline_value) {
    if (!c->struct_first_item) {
      if (!yaml_write_char(c, '\n')) return false;
    }
    c->struct_first_item = false;
    if (!yaml_write_indent(c)) return false;
  }

  if (!yaml_write_key_prefix(c)) return false;
  c->struct_inline_value = false;

  return yaml_write_str(c, value ? "true" : "false");
}

static bool yaml_write_list_begin(void* ctx, size_t size) {
  yaml_ctx_t* c = (yaml_ctx_t*)ctx;

  // Determine if we should use flow style (inline) or block style
  // Use flow style for small simple lists (size <= 8 and not nested)
  bool use_flow = (size <= 8) && !c->in_block_list;

  if (c->in_flow_list) {
    if (!c->flow_list_first_item) {
      if (!yaml_write_str(c, ", ")) return false;
    }
    c->flow_list_first_item = false;
    // Nested lists in flow context stay in flow
    use_flow = true;
  } else if (c->in_block_list) {
    if (!yaml_write_newline_indent(c)) return false;
    if (!yaml_write_str(c, "- ")) return false;
    // Nested lists in block context use block style
    use_flow = false;
  } else if (c->in_struct && !c->struct_inline_value) {
    if (!c->struct_first_item) {
      if (!yaml_write_char(c, '\n')) return false;
    }
    c->struct_first_item = false;
    if (!yaml_write_indent(c)) return false;
  }

  if (!yaml_write_key_prefix(c)) return false;
  c->struct_inline_value = false;

  if (use_flow) {
    if (!yaml_write_char(c, '[')) return false;
    c->in_flow_list = true;
    c->flow_list_first_item = true;
  } else {
    // Block style list - items will be on separate lines with "-"
    c->in_block_list = true;
    c->indent_level++;
  }

  return true;
}

static bool yaml_write_list_end(void* ctx) {
  yaml_ctx_t* c = (yaml_ctx_t*)ctx;

  if (c->in_flow_list) {
    if (!yaml_write_char(c, ']')) return false;
    c->in_flow_list = false;
  } else if (c->in_block_list) {
    c->indent_level--;
    c->in_block_list = false;
  }

  return true;
}

static bool yaml_write_struct_begin(void* ctx) {
  yaml_ctx_t* c = (yaml_ctx_t*)ctx;

  if (c->in_flow_list) {
    if (!c->flow_list_first_item) {
      if (!yaml_write_str(c, ", ")) return false;
    }
    c->flow_list_first_item = false;
    // Struct in flow list - use inline notation
    if (!yaml_write_char(c, '{')) return false;
    c->in_struct = true;
    c->struct_first_item = true;
    c->struct_inline_value = false;
    return true;
  }

  if (c->in_block_list) {
    if (!yaml_write_newline_indent(c)) return false;
    if (!yaml_write_str(c, "- ")) return false;
  } else if (c->in_struct && !c->struct_inline_value) {
    if (!c->struct_first_item) {
      if (!yaml_write_char(c, '\n')) return false;
    }
    c->struct_first_item = false;
    if (!yaml_write_indent(c)) return false;
  }

  if (!yaml_write_key_prefix(c)) return false;

  // Start new struct with indentation
  if (!yaml_write_char(c, '\n')) return false;
  c->indent_level++;
  c->in_struct = true;
  c->struct_first_item = true;
  c->struct_inline_value = false;
  return true;
}

static bool yaml_write_struct_key(void* ctx, const char* key) {
  yaml_ctx_t* c = (yaml_ctx_t*)ctx;
  c->pending_key = key;
  return true;
}

static bool yaml_write_struct_end(void* ctx) {
  yaml_ctx_t* c = (yaml_ctx_t*)ctx;

  if (c->in_flow_list) {
    // We're in an inline struct within a flow list
    if (!yaml_write_char(c, '}')) return false;
  } else {
    c->indent_level--;
  }
  c->in_struct = (c->indent_level > 0);
  c->struct_inline_value = false;
  return true;
}

static bool yaml_write_matrix(void* ctx, size_t ndims, const size_t* dims, const double* data) {
  yaml_ctx_t* c = (yaml_ctx_t*)ctx;

  if (c->in_flow_list) {
    if (!c->flow_list_first_item) {
      if (!yaml_write_str(c, ", ")) return false;
    }
    c->flow_list_first_item = false;
  } else if (c->in_block_list) {
    if (!yaml_write_newline_indent(c)) return false;
    if (!yaml_write_str(c, "- ")) return false;
  } else if (c->in_struct && !c->struct_inline_value) {
    if (!c->struct_first_item) {
      if (!yaml_write_char(c, '\n')) return false;
    }
    c->struct_first_item = false;
    if (!yaml_write_indent(c)) return false;
  }

  if (!yaml_write_key_prefix(c)) return false;
  c->struct_inline_value = false;

  // Write matrix with !matrix tag
  if (!yaml_write_str(c, "!matrix\n")) return false;
  c->indent_level++;

  // Write dims
  if (!yaml_write_indent(c)) return false;
  if (!yaml_write_str(c, "dims: [")) return false;
  for (size_t i = 0; i < ndims; i++) {
    if (i > 0) {
      if (!yaml_write_str(c, ", ")) return false;
    }
    char buf[32];
    snprintf(buf, sizeof(buf), "%zu", dims[i]);
    if (!yaml_write_str(c, buf)) return false;
  }
  if (!yaml_write_str(c, "]\n")) return false;

  // Write data
  if (!yaml_write_indent(c)) return false;
  if (!yaml_write_str(c, "data: [")) return false;

  size_t total = 1;
  for (size_t i = 0; i < ndims; i++) {
    total *= dims[i];
  }

  for (size_t i = 0; i < total; i++) {
    if (i > 0) {
      if (!yaml_write_str(c, ", ")) return false;
    }
    char buf[64];
    snprintf(buf, sizeof(buf), "%g", data[i]);
    if (!yaml_write_str(c, buf)) return false;
  }
  if (!yaml_write_char(c, ']')) return false;

  c->indent_level--;
  return true;
}

// #############################################################################
// Read helpers
// #############################################################################

// Get the indentation level of the current line
static int yaml_get_line_indent(text_parse_ctx_t* p) {
  // Find the start of the current line
  size_t line_start = p->pos;
  while (line_start > 0 && p->buffer[line_start - 1] != '\n') {
    line_start--;
  }

  // Count spaces at start of line (2 spaces = 1 indent level)
  int spaces = 0;
  size_t pos = line_start;
  while (pos < p->size && p->buffer[pos] == ' ') {
    spaces++;
    pos++;
  }
  return spaces / 2;
}

// Skip to the next line
static void yaml_skip_to_next_line(text_parse_ctx_t* p) {
  while (p->pos < p->size && p->buffer[p->pos] != '\n') {
    p->pos++;
  }
  if (p->pos < p->size) {
    p->pos++;  // Skip the newline
  }
}

// Parse an unquoted YAML string (until colon, comma, newline, or special chars)
static const char* yaml_parse_unquoted_value(text_parse_ctx_t* ctx) {
  text_parse_skip_whitespace(ctx);
  size_t start = ctx->pos;

  while (ctx->pos < ctx->size) {
    char c = ctx->buffer[ctx->pos];
    if (c == '\n' || c == '\r' || c == '#' || c == ',' ||
        c == ':' || c == '[' || c == ']' || c == '{' || c == '}') {
      break;
    }
    ctx->pos++;
  }

  // Trim trailing whitespace
  size_t end = ctx->pos;
  while (end > start && (ctx->buffer[end - 1] == ' ' || ctx->buffer[end - 1] == '\t')) {
    end--;
  }

  size_t len = end - start;
  if (len == 0) return NULL;

  if (!text_parse_ensure_temp(ctx, len)) return NULL;
  memcpy(ctx->temp_string, ctx->buffer + start, len);
  ctx->temp_string[len] = '\0';
  return ctx->temp_string;
}

// #############################################################################
// Read callbacks
// #############################################################################

static olib_object_type_t yaml_read_peek(void* ctx) {
  yaml_ctx_t* c = (yaml_ctx_t*)ctx;
  text_parse_ctx_t* p = &c->parse;
  text_parse_skip_whitespace_and_comments(p);

  // Skip comma if present (between flow list/object elements)
  if (p->pos < p->size && p->buffer[p->pos] == ',') {
    p->pos++;
    text_parse_skip_whitespace_and_comments(p);
  }

  if (text_parse_eof(p)) {
    return OLIB_OBJECT_TYPE_MAX;
  }

  char ch = text_parse_peek_raw(p);

  // Check for block list item
  if (ch == '-') {
    // Check if it's "- " (list item) or a negative number
    if (p->pos + 1 < p->size && (p->buffer[p->pos + 1] == ' ' || p->buffer[p->pos + 1] == '\n')) {
      return OLIB_OBJECT_TYPE_LIST;
    }
  }

  // Flow list
  if (ch == '[') {
    return OLIB_OBJECT_TYPE_LIST;
  }

  // Flow/block mapping
  if (ch == '{') {
    return OLIB_OBJECT_TYPE_STRUCT;
  }

  // Quoted string
  if (ch == '"' || ch == '\'') {
    return OLIB_OBJECT_TYPE_STRING;
  }

  // Check for !matrix tag
  if (ch == '!') {
    if (p->pos + 7 <= p->size && strncmp(p->buffer + p->pos, "!matrix", 7) == 0) {
      return OLIB_OBJECT_TYPE_MATRIX;
    }
  }

  // Number (possibly negative)
  if (ch == '-' || ch == '+' || isdigit((unsigned char)ch)) {
    // Look ahead to see if it's int or float
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
  if (ch == 't' || ch == 'T') {
    if ((p->pos + 4 <= p->size && strncmp(p->buffer + p->pos, "true", 4) == 0) ||
        (p->pos + 4 <= p->size && strncmp(p->buffer + p->pos, "True", 4) == 0) ||
        (p->pos + 4 <= p->size && strncmp(p->buffer + p->pos, "TRUE", 4) == 0)) {
      return OLIB_OBJECT_TYPE_BOOL;
    }
  }
  if (ch == 'f' || ch == 'F') {
    if ((p->pos + 5 <= p->size && strncmp(p->buffer + p->pos, "false", 5) == 0) ||
        (p->pos + 5 <= p->size && strncmp(p->buffer + p->pos, "False", 5) == 0) ||
        (p->pos + 5 <= p->size && strncmp(p->buffer + p->pos, "FALSE", 5) == 0)) {
      return OLIB_OBJECT_TYPE_BOOL;
    }
  }
  if (ch == 'y' || ch == 'Y') {
    if ((p->pos + 3 <= p->size && strncmp(p->buffer + p->pos, "yes", 3) == 0) ||
        (p->pos + 3 <= p->size && strncmp(p->buffer + p->pos, "Yes", 3) == 0) ||
        (p->pos + 3 <= p->size && strncmp(p->buffer + p->pos, "YES", 3) == 0)) {
      return OLIB_OBJECT_TYPE_BOOL;
    }
  }
  if (ch == 'n' || ch == 'N') {
    if ((p->pos + 2 <= p->size && strncmp(p->buffer + p->pos, "no", 2) == 0) ||
        (p->pos + 2 <= p->size && strncmp(p->buffer + p->pos, "No", 2) == 0) ||
        (p->pos + 2 <= p->size && strncmp(p->buffer + p->pos, "NO", 2) == 0)) {
      return OLIB_OBJECT_TYPE_BOOL;
    }
  }
  if (ch == 'o' || ch == 'O') {
    if ((p->pos + 2 <= p->size && strncmp(p->buffer + p->pos, "on", 2) == 0) ||
        (p->pos + 2 <= p->size && strncmp(p->buffer + p->pos, "On", 2) == 0) ||
        (p->pos + 2 <= p->size && strncmp(p->buffer + p->pos, "ON", 2) == 0)) {
      return OLIB_OBJECT_TYPE_BOOL;
    }
    if ((p->pos + 3 <= p->size && strncmp(p->buffer + p->pos, "off", 3) == 0) ||
        (p->pos + 3 <= p->size && strncmp(p->buffer + p->pos, "Off", 3) == 0) ||
        (p->pos + 3 <= p->size && strncmp(p->buffer + p->pos, "OFF", 3) == 0)) {
      return OLIB_OBJECT_TYPE_BOOL;
    }
  }

  // Check if this is a key (has a colon) -> struct
  size_t pos = p->pos;
  while (pos < p->size && p->buffer[pos] != '\n' && p->buffer[pos] != '#') {
    if (p->buffer[pos] == ':') {
      // Make sure it's followed by space or newline
      if (pos + 1 < p->size && (p->buffer[pos + 1] == ' ' || p->buffer[pos + 1] == '\n')) {
        return OLIB_OBJECT_TYPE_STRUCT;
      }
    }
    pos++;
  }

  // Default to string
  return OLIB_OBJECT_TYPE_STRING;
}

static bool yaml_read_int(void* ctx, int64_t* value) {
  yaml_ctx_t* c = (yaml_ctx_t*)ctx;
  text_parse_number_result_t result;
  if (!text_parse_number(&c->parse, &result)) return false;
  *value = result.int_value;
  return true;
}

static bool yaml_read_uint(void* ctx, uint64_t* value) {
  yaml_ctx_t* c = (yaml_ctx_t*)ctx;
  text_parse_number_result_t result;
  if (!text_parse_number(&c->parse, &result)) return false;
  *value = (uint64_t)result.int_value;
  return true;
}

static bool yaml_read_float(void* ctx, double* value) {
  yaml_ctx_t* c = (yaml_ctx_t*)ctx;
  text_parse_number_result_t result;
  if (!text_parse_number(&c->parse, &result)) return false;
  *value = result.is_float ? result.float_value : (double)result.int_value;
  return true;
}

static bool yaml_read_string(void* ctx, const char** value) {
  yaml_ctx_t* c = (yaml_ctx_t*)ctx;
  text_parse_ctx_t* p = &c->parse;
  text_parse_skip_whitespace(p);

  char ch = text_parse_peek_raw(p);

  if (ch == '"') {
    const char* str = text_parse_quoted_string(p);
    if (!str) return false;
    *value = str;
    return true;
  }

  if (ch == '\'') {
    const char* str = text_parse_single_quoted_string(p);
    if (!str) return false;
    *value = str;
    return true;
  }

  // Unquoted string
  const char* str = yaml_parse_unquoted_value(p);
  if (!str) return false;
  *value = str;
  return true;
}

static bool yaml_read_bool(void* ctx, bool* value) {
  yaml_ctx_t* c = (yaml_ctx_t*)ctx;
  text_parse_ctx_t* p = &c->parse;
  text_parse_skip_whitespace(p);

  // True values
  if (text_parse_match_str(p, "true") || text_parse_match_str(p, "True") ||
      text_parse_match_str(p, "TRUE") || text_parse_match_str(p, "yes") ||
      text_parse_match_str(p, "Yes") || text_parse_match_str(p, "YES") ||
      text_parse_match_str(p, "on") || text_parse_match_str(p, "On") ||
      text_parse_match_str(p, "ON")) {
    *value = true;
    return true;
  }

  // False values
  if (text_parse_match_str(p, "false") || text_parse_match_str(p, "False") ||
      text_parse_match_str(p, "FALSE") || text_parse_match_str(p, "no") ||
      text_parse_match_str(p, "No") || text_parse_match_str(p, "NO") ||
      text_parse_match_str(p, "off") || text_parse_match_str(p, "Off") ||
      text_parse_match_str(p, "OFF")) {
    *value = false;
    return true;
  }

  return false;
}

static bool yaml_read_list_begin(void* ctx, size_t* size) {
  yaml_ctx_t* c = (yaml_ctx_t*)ctx;
  text_parse_ctx_t* p = &c->parse;
  text_parse_skip_whitespace_and_comments(p);

  char ch = text_parse_peek_raw(p);

  // Flow list [...]
  if (ch == '[') {
    if (!text_parse_match(p, '[')) return false;

    // Count elements by scanning ahead
    size_t pos = p->pos;
    size_t count = 0;
    int depth = 1;
    bool has_content = false;

    while (pos < p->size && depth > 0) {
      char c = p->buffer[pos];
      if (c == '[' || c == '{') {
        depth++;
        has_content = true;
      } else if (c == ']' || c == '}') {
        depth--;
      } else if (depth == 1 && c == ',') {
        count++;
      } else if (c != ' ' && c != '\t' && c != '\n' && c != '\r' && depth == 1) {
        has_content = true;
      }
      pos++;
    }

    if (has_content) {
      count++;
    }

    *size = count;
    return true;
  }

  // Block list (starts with "- ")
  if (ch == '-') {
    // Count items at this indentation level
    int base_indent = yaml_get_line_indent(p);
    size_t count = 0;
    size_t pos = p->pos;

    while (pos < p->size) {
      // Skip whitespace at start of line
      size_t line_start = pos;
      int spaces = 0;
      while (pos < p->size && p->buffer[pos] == ' ') {
        spaces++;
        pos++;
      }
      int current_indent = spaces / 2;

      if (current_indent < base_indent) {
        break;  // Dedent
      }

      if (current_indent == base_indent && pos < p->size && p->buffer[pos] == '-') {
        if (pos + 1 < p->size && (p->buffer[pos + 1] == ' ' || p->buffer[pos + 1] == '\n')) {
          count++;
        }
      }

      // Skip to next line
      while (pos < p->size && p->buffer[pos] != '\n') {
        pos++;
      }
      if (pos < p->size) pos++;  // Skip newline
    }

    *size = count;
    return true;
  }

  return false;
}

static bool yaml_read_list_end(void* ctx) {
  yaml_ctx_t* c = (yaml_ctx_t*)ctx;
  text_parse_ctx_t* p = &c->parse;
  text_parse_skip_whitespace(p);

  // For flow lists, consume the closing bracket
  if (text_parse_peek_raw(p) == ']') {
    text_parse_match(p, ',');  // Optional trailing comma
    return text_parse_match(p, ']');
  }

  // For block lists, we don't consume anything - we just return true
  // The caller handles the indentation-based end detection
  return true;
}

static bool yaml_read_struct_begin(void* ctx) {
  yaml_ctx_t* c = (yaml_ctx_t*)ctx;
  text_parse_ctx_t* p = &c->parse;
  text_parse_skip_whitespace_and_comments(p);

  // Flow mapping {...}
  if (text_parse_peek_raw(p) == '{') {
    return text_parse_match(p, '{');
  }

  // Block mapping - no delimiter to consume
  return true;
}

static bool yaml_read_struct_key(void* ctx, const char** key) {
  yaml_ctx_t* c = (yaml_ctx_t*)ctx;
  text_parse_ctx_t* p = &c->parse;
  text_parse_skip_whitespace_and_comments(p);

  // Check for end of mapping
  char ch = text_parse_peek_raw(p);
  if (ch == '}' || ch == ']' || ch == '\0') {
    return false;
  }

  // Skip "- " if this is an list item containing a mapping
  if (ch == '-' && p->pos + 1 < p->size &&
      (p->buffer[p->pos + 1] == ' ' || p->buffer[p->pos + 1] == '\n')) {
    p->pos += 2;
    text_parse_skip_whitespace(p);
  }

  // Read the key
  if (text_parse_peek_raw(p) == '"') {
    const char* str = text_parse_quoted_string(p);
    if (!str) return false;
    *key = str;
  } else if (text_parse_peek_raw(p) == '\'') {
    const char* str = text_parse_single_quoted_string(p);
    if (!str) return false;
    *key = str;
  } else {
    // Unquoted key
    const char* id = text_parse_identifier(p);
    if (!id) return false;
    *key = id;
  }

  // Skip the colon
  text_parse_skip_whitespace(p);
  if (!text_parse_match(p, ':')) return false;

  return true;
}

static bool yaml_read_struct_end(void* ctx) {
  yaml_ctx_t* c = (yaml_ctx_t*)ctx;
  text_parse_ctx_t* p = &c->parse;
  text_parse_skip_whitespace(p);

  // For flow mappings, consume the closing brace
  if (text_parse_peek_raw(p) == '}') {
    text_parse_match(p, ',');  // Optional trailing comma
    return text_parse_match(p, '}');
  }

  // For block mappings, no delimiter to consume
  return true;
}

static bool yaml_read_matrix(void* ctx, size_t* ndims, size_t** dims, double** data) {
  yaml_ctx_t* c = (yaml_ctx_t*)ctx;
  text_parse_ctx_t* p = &c->parse;
  text_parse_skip_whitespace_and_comments(p);

  // Check for !matrix tag
  if (!text_parse_match_str(p, "!matrix")) {
    return false;
  }

  text_parse_skip_whitespace_and_comments(p);

  // Parse dims
  if (!text_parse_match_str(p, "dims")) return false;
  if (!text_parse_match(p, ':')) return false;
  if (!text_parse_match(p, '[')) return false;

  size_t dim_count = 0;
  size_t dim_cap = 4;
  size_t* d = olib_malloc(dim_cap * sizeof(size_t));
  if (!d) return false;

  while (text_parse_peek(p) != ']') {
    if (dim_count > 0) text_parse_match(p, ',');

    text_parse_number_result_t result;
    if (!text_parse_number(p, &result)) {
      olib_free(d);
      return false;
    }

    if (dim_count >= dim_cap) {
      dim_cap *= 2;
      size_t* new_d = olib_realloc(d, dim_cap * sizeof(size_t));
      if (!new_d) {
        olib_free(d);
        return false;
      }
      d = new_d;
    }
    d[dim_count++] = (size_t)result.int_value;
  }

  if (!text_parse_match(p, ']')) {
    olib_free(d);
    return false;
  }

  text_parse_skip_whitespace_and_comments(p);

  // Parse data
  if (!text_parse_match_str(p, "data")) {
    olib_free(d);
    return false;
  }
  if (!text_parse_match(p, ':')) {
    olib_free(d);
    return false;
  }
  if (!text_parse_match(p, '[')) {
    olib_free(d);
    return false;
  }

  size_t total = 1;
  for (size_t i = 0; i < dim_count; i++) {
    total *= d[i];
  }

  double* values = olib_malloc(total * sizeof(double));
  if (!values) {
    olib_free(d);
    return false;
  }

  for (size_t i = 0; i < total; i++) {
    if (i > 0) text_parse_match(p, ',');
    text_parse_number_result_t result;
    if (!text_parse_number(p, &result)) {
      olib_free(d);
      olib_free(values);
      return false;
    }
    values[i] = result.is_float ? result.float_value : (double)result.int_value;
  }

  if (!text_parse_match(p, ']')) {
    olib_free(d);
    olib_free(values);
    return false;
  }

  *ndims = dim_count;
  *dims = d;
  *data = values;
  return true;
}

// #############################################################################
// Lifecycle callbacks
// #############################################################################

static void yaml_free_ctx(void* ctx) {
  yaml_ctx_t* c = (yaml_ctx_t*)ctx;
  if (c->write_buffer) olib_free(c->write_buffer);
  text_parse_free(&c->parse);
  olib_free(c);
}

static bool yaml_init_write(void* ctx) {
  yaml_ctx_t* c = (yaml_ctx_t*)ctx;
  c->write_size = 0;
  c->indent_level = 0;
  c->in_flow_list = false;
  c->flow_list_first_item = true;
  c->in_block_list = false;
  c->block_list_needs_newline = false;
  c->in_struct = false;
  c->struct_first_item = true;
  c->struct_inline_value = false;
  c->pending_key = NULL;
  return true;
}

static bool yaml_finish_write(void* ctx, uint8_t** out_data, size_t* out_size) {
  yaml_ctx_t* c = (yaml_ctx_t*)ctx;
  if (!out_data || !out_size) {
    return false;
  }

  // Add null terminator
  if (!yaml_ensure_write_capacity(c, 1)) return false;
  c->write_buffer[c->write_size] = '\0';

  *out_data = (uint8_t*)c->write_buffer;
  *out_size = c->write_size;

  c->write_buffer = NULL;
  c->write_capacity = 0;
  c->write_size = 0;
  return true;
}

static bool yaml_init_read(void* ctx, const uint8_t* data, size_t size) {
  yaml_ctx_t* c = (yaml_ctx_t*)ctx;
  text_parse_init(&c->parse, (const char*)data, size);
  c->read_indent_level = 0;
  return true;
}

static bool yaml_finish_read(void* ctx) {
  yaml_ctx_t* c = (yaml_ctx_t*)ctx;
  text_parse_reset(&c->parse);
  return true;
}

// #############################################################################
// Public API
// #############################################################################

OLIB_API olib_serializer_t* olib_serializer_new_yaml() {
  yaml_ctx_t* ctx = olib_calloc(1, sizeof(yaml_ctx_t));
  if (!ctx) {
    return NULL;
  }

  olib_serializer_config_t config = {
    .user_data = ctx,
    .text_based = true,
    .free_ctx = yaml_free_ctx,
    .init_write = yaml_init_write,
    .finish_write = yaml_finish_write,
    .init_read = yaml_init_read,
    .finish_read = yaml_finish_read,

    .write_int = yaml_write_int,
    .write_uint = yaml_write_uint,
    .write_float = yaml_write_float,
    .write_string = yaml_write_string,
    .write_bool = yaml_write_bool,
    .write_list_begin = yaml_write_list_begin,
    .write_list_end = yaml_write_list_end,
    .write_struct_begin = yaml_write_struct_begin,
    .write_struct_key = yaml_write_struct_key,
    .write_struct_end = yaml_write_struct_end,
    .write_matrix = yaml_write_matrix,

    .read_peek = yaml_read_peek,
    .read_int = yaml_read_int,
    .read_uint = yaml_read_uint,
    .read_float = yaml_read_float,
    .read_string = yaml_read_string,
    .read_bool = yaml_read_bool,
    .read_list_begin = yaml_read_list_begin,
    .read_list_end = yaml_read_list_end,
    .read_struct_begin = yaml_read_struct_begin,
    .read_struct_key = yaml_read_struct_key,
    .read_struct_end = yaml_read_struct_end,
    .read_matrix = yaml_read_matrix,
  };

  olib_serializer_t* serializer = olib_serializer_new(&config);
  if (!serializer) {
    olib_free(ctx);
    return NULL;
  }

  return serializer;
}
