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
#include <string.h>

// #############################################################################
// BSON-like format type tags
// #############################################################################

#define JSONB_TAG_INT    0x01  // int64 (8 bytes little-endian)
#define JSONB_TAG_UINT   0x02  // uint64 (8 bytes little-endian)
#define JSONB_TAG_FLOAT  0x03  // double (8 bytes IEEE 754 little-endian)
#define JSONB_TAG_STRING 0x04  // string (4-byte length + UTF-8 data, no null terminator)
#define JSONB_TAG_BOOL   0x05  // bool (1 byte: 0 or 1)
#define JSONB_TAG_ARRAY  0x06  // array (4-byte count + elements)
#define JSONB_TAG_STRUCT 0x07  // struct (key-value pairs, ends with 0-length key)
#define JSONB_TAG_MATRIX 0x08  // matrix (4-byte ndims + dims[] + data[])

// #############################################################################
// Context structure for JSON binary serialization
// #############################################################################

typedef struct {
  // Write mode
  uint8_t* write_buffer;
  size_t write_capacity;
  size_t write_size;

  // Read mode
  const uint8_t* read_buffer;
  size_t read_size;
  size_t read_pos;

  // Temporary string storage for read_string/read_struct_key
  char* temp_string;
  size_t temp_string_capacity;
} jsonb_ctx_t;

// #############################################################################
// Endian-independent encoding helpers (little-endian wire format)
// #############################################################################

static bool jsonb_ensure_write_capacity(jsonb_ctx_t* ctx, size_t needed) {
  size_t required = ctx->write_size + needed;
  if (required <= ctx->write_capacity) {
    return true;
  }

  size_t new_capacity = ctx->write_capacity ? ctx->write_capacity * 2 : 256;
  while (new_capacity < required) {
    new_capacity *= 2;
  }

  uint8_t* new_buffer = olib_realloc(ctx->write_buffer, new_capacity);
  if (!new_buffer) {
    return false;
  }

  ctx->write_buffer = new_buffer;
  ctx->write_capacity = new_capacity;
  return true;
}

static bool jsonb_write_u8(jsonb_ctx_t* ctx, uint8_t value) {
  if (!jsonb_ensure_write_capacity(ctx, 1)) return false;
  ctx->write_buffer[ctx->write_size++] = value;
  return true;
}

static bool jsonb_write_u32(jsonb_ctx_t* ctx, uint32_t value) {
  if (!jsonb_ensure_write_capacity(ctx, 4)) return false;
  ctx->write_buffer[ctx->write_size++] = (uint8_t)(value & 0xFF);
  ctx->write_buffer[ctx->write_size++] = (uint8_t)((value >> 8) & 0xFF);
  ctx->write_buffer[ctx->write_size++] = (uint8_t)((value >> 16) & 0xFF);
  ctx->write_buffer[ctx->write_size++] = (uint8_t)((value >> 24) & 0xFF);
  return true;
}

static bool jsonb_write_u64(jsonb_ctx_t* ctx, uint64_t value) {
  if (!jsonb_ensure_write_capacity(ctx, 8)) return false;
  for (int i = 0; i < 8; i++) {
    ctx->write_buffer[ctx->write_size++] = (uint8_t)((value >> (i * 8)) & 0xFF);
  }
  return true;
}

static bool jsonb_write_i64(jsonb_ctx_t* ctx, int64_t value) {
  return jsonb_write_u64(ctx, (uint64_t)value);
}

static bool jsonb_write_f64(jsonb_ctx_t* ctx, double value) {
  uint64_t bits;
  memcpy(&bits, &value, sizeof(bits));
  return jsonb_write_u64(ctx, bits);
}

static bool jsonb_write_bytes(jsonb_ctx_t* ctx, const uint8_t* data, size_t len) {
  if (!jsonb_ensure_write_capacity(ctx, len)) return false;
  memcpy(ctx->write_buffer + ctx->write_size, data, len);
  ctx->write_size += len;
  return true;
}

// #############################################################################
// Endian-independent decoding helpers
// #############################################################################

static bool jsonb_read_u8(jsonb_ctx_t* ctx, uint8_t* out) {
  if (ctx->read_pos + 1 > ctx->read_size) return false;
  *out = ctx->read_buffer[ctx->read_pos++];
  return true;
}

static bool jsonb_read_u32(jsonb_ctx_t* ctx, uint32_t* out) {
  if (ctx->read_pos + 4 > ctx->read_size) return false;
  *out = (uint32_t)ctx->read_buffer[ctx->read_pos]
       | ((uint32_t)ctx->read_buffer[ctx->read_pos + 1] << 8)
       | ((uint32_t)ctx->read_buffer[ctx->read_pos + 2] << 16)
       | ((uint32_t)ctx->read_buffer[ctx->read_pos + 3] << 24);
  ctx->read_pos += 4;
  return true;
}

static bool jsonb_read_u64(jsonb_ctx_t* ctx, uint64_t* out) {
  if (ctx->read_pos + 8 > ctx->read_size) return false;
  *out = 0;
  for (int i = 0; i < 8; i++) {
    *out |= ((uint64_t)ctx->read_buffer[ctx->read_pos + i] << (i * 8));
  }
  ctx->read_pos += 8;
  return true;
}

static bool jsonb_read_i64(jsonb_ctx_t* ctx, int64_t* out) {
  uint64_t value;
  if (!jsonb_read_u64(ctx, &value)) return false;
  *out = (int64_t)value;
  return true;
}

static bool jsonb_read_f64(jsonb_ctx_t* ctx, double* out) {
  uint64_t bits;
  if (!jsonb_read_u64(ctx, &bits)) return false;
  memcpy(out, &bits, sizeof(*out));
  return true;
}

// #############################################################################
// Write callbacks
// #############################################################################

static bool jsonb_write_int(void* ctx, int64_t value) {
  jsonb_ctx_t* c = (jsonb_ctx_t*)ctx;
  if (!jsonb_write_u8(c, JSONB_TAG_INT)) return false;
  return jsonb_write_i64(c, value);
}

static bool jsonb_write_uint(void* ctx, uint64_t value) {
  jsonb_ctx_t* c = (jsonb_ctx_t*)ctx;
  if (!jsonb_write_u8(c, JSONB_TAG_UINT)) return false;
  return jsonb_write_u64(c, value);
}

static bool jsonb_write_float(void* ctx, double value) {
  jsonb_ctx_t* c = (jsonb_ctx_t*)ctx;
  if (!jsonb_write_u8(c, JSONB_TAG_FLOAT)) return false;
  return jsonb_write_f64(c, value);
}

static bool jsonb_write_string(void* ctx, const char* value) {
  jsonb_ctx_t* c = (jsonb_ctx_t*)ctx;
  if (!jsonb_write_u8(c, JSONB_TAG_STRING)) return false;
  uint32_t len = value ? (uint32_t)strlen(value) : 0;
  if (!jsonb_write_u32(c, len)) return false;
  if (len > 0) {
    if (!jsonb_write_bytes(c, (const uint8_t*)value, len)) return false;
  }
  return true;
}

static bool jsonb_write_bool(void* ctx, bool value) {
  jsonb_ctx_t* c = (jsonb_ctx_t*)ctx;
  if (!jsonb_write_u8(c, JSONB_TAG_BOOL)) return false;
  return jsonb_write_u8(c, value ? 1 : 0);
}

static bool jsonb_write_array_begin(void* ctx, size_t size) {
  jsonb_ctx_t* c = (jsonb_ctx_t*)ctx;
  if (!jsonb_write_u8(c, JSONB_TAG_ARRAY)) return false;
  return jsonb_write_u32(c, (uint32_t)size);
}

static bool jsonb_write_array_end(void* ctx) {
  (void)ctx;
  return true;
}

static bool jsonb_write_struct_begin(void* ctx) {
  jsonb_ctx_t* c = (jsonb_ctx_t*)ctx;
  return jsonb_write_u8(c, JSONB_TAG_STRUCT);
}

static bool jsonb_write_struct_key(void* ctx, const char* key) {
  jsonb_ctx_t* c = (jsonb_ctx_t*)ctx;
  uint32_t len = key ? (uint32_t)strlen(key) : 0;
  if (!jsonb_write_u32(c, len)) return false;
  if (len > 0) {
    if (!jsonb_write_bytes(c, (const uint8_t*)key, len)) return false;
  }
  return true;
}

static bool jsonb_write_struct_end(void* ctx) {
  jsonb_ctx_t* c = (jsonb_ctx_t*)ctx;
  // Write zero-length key to mark end of struct
  return jsonb_write_u32(c, 0);
}

static bool jsonb_write_matrix(void* ctx, size_t ndims, const size_t* dims, const double* data) {
  jsonb_ctx_t* c = (jsonb_ctx_t*)ctx;
  if (!jsonb_write_u8(c, JSONB_TAG_MATRIX)) return false;
  if (!jsonb_write_u32(c, (uint32_t)ndims)) return false;

  size_t total = 1;
  for (size_t i = 0; i < ndims; i++) {
    if (!jsonb_write_u32(c, (uint32_t)dims[i])) return false;
    total *= dims[i];
  }

  for (size_t i = 0; i < total; i++) {
    if (!jsonb_write_f64(c, data[i])) return false;
  }

  return true;
}

// #############################################################################
// Read callbacks
// #############################################################################

static olib_object_type_t jsonb_read_peek(void* ctx) {
  jsonb_ctx_t* c = (jsonb_ctx_t*)ctx;
  if (c->read_pos >= c->read_size) {
    return OLIB_OBJECT_TYPE_MAX;
  }

  uint8_t tag = c->read_buffer[c->read_pos];
  switch (tag) {
    case JSONB_TAG_INT:    return OLIB_OBJECT_TYPE_INT;
    case JSONB_TAG_UINT:   return OLIB_OBJECT_TYPE_UINT;
    case JSONB_TAG_FLOAT:  return OLIB_OBJECT_TYPE_FLOAT;
    case JSONB_TAG_STRING: return OLIB_OBJECT_TYPE_STRING;
    case JSONB_TAG_BOOL:   return OLIB_OBJECT_TYPE_BOOL;
    case JSONB_TAG_ARRAY:  return OLIB_OBJECT_TYPE_ARRAY;
    case JSONB_TAG_STRUCT: return OLIB_OBJECT_TYPE_STRUCT;
    case JSONB_TAG_MATRIX: return OLIB_OBJECT_TYPE_MATRIX;
    default:               return OLIB_OBJECT_TYPE_MAX;
  }
}

static bool jsonb_read_int(void* ctx, int64_t* value) {
  jsonb_ctx_t* c = (jsonb_ctx_t*)ctx;
  uint8_t tag;
  if (!jsonb_read_u8(c, &tag) || tag != JSONB_TAG_INT) return false;
  return jsonb_read_i64(c, value);
}

static bool jsonb_read_uint(void* ctx, uint64_t* value) {
  jsonb_ctx_t* c = (jsonb_ctx_t*)ctx;
  uint8_t tag;
  if (!jsonb_read_u8(c, &tag) || tag != JSONB_TAG_UINT) return false;
  return jsonb_read_u64(c, value);
}

static bool jsonb_read_float(void* ctx, double* value) {
  jsonb_ctx_t* c = (jsonb_ctx_t*)ctx;
  uint8_t tag;
  if (!jsonb_read_u8(c, &tag) || tag != JSONB_TAG_FLOAT) return false;
  return jsonb_read_f64(c, value);
}

static bool jsonb_ensure_temp_string(jsonb_ctx_t* ctx, size_t len) {
  size_t needed = len + 1;
  if (needed <= ctx->temp_string_capacity) {
    return true;
  }

  char* new_buf = olib_realloc(ctx->temp_string, needed);
  if (!new_buf) return false;

  ctx->temp_string = new_buf;
  ctx->temp_string_capacity = needed;
  return true;
}

static bool jsonb_read_string(void* ctx, const char** value) {
  jsonb_ctx_t* c = (jsonb_ctx_t*)ctx;
  uint8_t tag;
  if (!jsonb_read_u8(c, &tag) || tag != JSONB_TAG_STRING) return false;

  uint32_t len;
  if (!jsonb_read_u32(c, &len)) return false;

  if (!jsonb_ensure_temp_string(c, len)) return false;

  if (len > 0) {
    if (c->read_pos + len > c->read_size) return false;
    memcpy(c->temp_string, c->read_buffer + c->read_pos, len);
    c->read_pos += len;
  }
  c->temp_string[len] = '\0';

  *value = c->temp_string;
  return true;
}

static bool jsonb_read_bool(void* ctx, bool* value) {
  jsonb_ctx_t* c = (jsonb_ctx_t*)ctx;
  uint8_t tag;
  if (!jsonb_read_u8(c, &tag) || tag != JSONB_TAG_BOOL) return false;
  uint8_t b;
  if (!jsonb_read_u8(c, &b)) return false;
  *value = (b != 0);
  return true;
}

static bool jsonb_read_array_begin(void* ctx, size_t* size) {
  jsonb_ctx_t* c = (jsonb_ctx_t*)ctx;
  uint8_t tag;
  if (!jsonb_read_u8(c, &tag) || tag != JSONB_TAG_ARRAY) return false;
  uint32_t count;
  if (!jsonb_read_u32(c, &count)) return false;
  *size = count;
  return true;
}

static bool jsonb_read_array_end(void* ctx) {
  (void)ctx;
  return true;
}

static bool jsonb_read_struct_begin(void* ctx) {
  jsonb_ctx_t* c = (jsonb_ctx_t*)ctx;
  uint8_t tag;
  if (!jsonb_read_u8(c, &tag) || tag != JSONB_TAG_STRUCT) return false;
  return true;
}

static bool jsonb_read_struct_key(void* ctx, const char** key) {
  jsonb_ctx_t* c = (jsonb_ctx_t*)ctx;

  uint32_t len;
  if (!jsonb_read_u32(c, &len)) return false;

  // Zero-length key marks end of struct
  if (len == 0) {
    // Rewind so we can read it again in read_struct_end
    c->read_pos -= 4;
    return false;
  }

  if (!jsonb_ensure_temp_string(c, len)) return false;

  if (c->read_pos + len > c->read_size) return false;
  memcpy(c->temp_string, c->read_buffer + c->read_pos, len);
  c->read_pos += len;
  c->temp_string[len] = '\0';

  *key = c->temp_string;
  return true;
}

static bool jsonb_read_struct_end(void* ctx) {
  jsonb_ctx_t* c = (jsonb_ctx_t*)ctx;
  uint32_t len;
  if (!jsonb_read_u32(c, &len)) return false;
  return (len == 0);
}

static bool jsonb_read_matrix(void* ctx, size_t* ndims, size_t** dims, double** data) {
  jsonb_ctx_t* c = (jsonb_ctx_t*)ctx;
  uint8_t tag;
  if (!jsonb_read_u8(c, &tag) || tag != JSONB_TAG_MATRIX) return false;

  uint32_t nd;
  if (!jsonb_read_u32(c, &nd)) return false;

  size_t* d = olib_malloc(nd * sizeof(size_t));
  if (!d) return false;

  size_t total = 1;
  for (uint32_t i = 0; i < nd; i++) {
    uint32_t dim;
    if (!jsonb_read_u32(c, &dim)) {
      olib_free(d);
      return false;
    }
    d[i] = dim;
    total *= dim;
  }

  double* values = olib_malloc(total * sizeof(double));
  if (!values) {
    olib_free(d);
    return false;
  }

  for (size_t i = 0; i < total; i++) {
    if (!jsonb_read_f64(c, &values[i])) {
      olib_free(d);
      olib_free(values);
      return false;
    }
  }

  *ndims = nd;
  *dims = d;
  *data = values;
  return true;
}

// #############################################################################
// Lifecycle callbacks
// #############################################################################

static void jsonb_free_ctx(void* ctx) {
  jsonb_ctx_t* c = (jsonb_ctx_t*)ctx;
  if (c->write_buffer) olib_free(c->write_buffer);
  if (c->temp_string) olib_free(c->temp_string);
  olib_free(c);
}

static bool jsonb_init_write(void* ctx) {
  jsonb_ctx_t* c = (jsonb_ctx_t*)ctx;
  // Reset write state for new serialization
  c->write_size = 0;
  return true;
}

static bool jsonb_finish_write(void* ctx, uint8_t** out_data, size_t* out_size) {
  jsonb_ctx_t* c = (jsonb_ctx_t*)ctx;
  if (!out_data || !out_size) {
    return false;
  }
  // Transfer ownership of the buffer to caller
  *out_data = c->write_buffer;
  *out_size = c->write_size;
  // Reset write state (buffer is now owned by caller)
  c->write_buffer = NULL;
  c->write_capacity = 0;
  c->write_size = 0;
  return true;
}

static bool jsonb_init_read(void* ctx, const uint8_t* data, size_t size) {
  jsonb_ctx_t* c = (jsonb_ctx_t*)ctx;
  c->read_buffer = data;
  c->read_size = size;
  c->read_pos = 0;
  return true;
}

static bool jsonb_finish_read(void* ctx) {
  jsonb_ctx_t* c = (jsonb_ctx_t*)ctx;
  // Reset read state
  c->read_buffer = NULL;
  c->read_size = 0;
  c->read_pos = 0;
  return true;
}

// #############################################################################
// Public API
// #############################################################################

OLIB_API olib_serializer_t* olib_serializer_new_json_binary() {
  jsonb_ctx_t* ctx = olib_calloc(1, sizeof(jsonb_ctx_t));
  if (!ctx) {
    return NULL;
  }

  olib_serializer_config_t config = {
    .user_data = ctx,
    .text_based = false,
    .free_ctx = jsonb_free_ctx,
    .init_write = jsonb_init_write,
    .finish_write = jsonb_finish_write,
    .init_read = jsonb_init_read,
    .finish_read = jsonb_finish_read,

    .write_int = jsonb_write_int,
    .write_uint = jsonb_write_uint,
    .write_float = jsonb_write_float,
    .write_string = jsonb_write_string,
    .write_bool = jsonb_write_bool,
    .write_array_begin = jsonb_write_array_begin,
    .write_array_end = jsonb_write_array_end,
    .write_struct_begin = jsonb_write_struct_begin,
    .write_struct_key = jsonb_write_struct_key,
    .write_struct_end = jsonb_write_struct_end,
    .write_matrix = jsonb_write_matrix,

    .read_peek = jsonb_read_peek,
    .read_int = jsonb_read_int,
    .read_uint = jsonb_read_uint,
    .read_float = jsonb_read_float,
    .read_string = jsonb_read_string,
    .read_bool = jsonb_read_bool,
    .read_array_begin = jsonb_read_array_begin,
    .read_array_end = jsonb_read_array_end,
    .read_struct_begin = jsonb_read_struct_begin,
    .read_struct_key = jsonb_read_struct_key,
    .read_struct_end = jsonb_read_struct_end,
    .read_matrix = jsonb_read_matrix,
  };

  olib_serializer_t* serializer = olib_serializer_new(&config);
  if (!serializer) {
    olib_free(ctx);
    return NULL;
  }

  return serializer;
}
