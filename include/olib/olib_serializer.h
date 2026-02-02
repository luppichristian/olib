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

#include "olib_object.h"

// #############################################################################
OLIB_HEADER_BEGIN;
// #############################################################################

typedef struct olib_serializer_t olib_serializer_t;

typedef struct olib_serializer_config_t {
  // Internal user data pointer for serializer context
  void* user_data;

  // Are we a text-based serializer?
  bool text_based;

  // Lifecycle callbacks
  void (*init_ctx)(void* ctx);                                            // Called after serializer creation (optional initialization)
  void (*free_ctx)(void* ctx);                                            // Called when serializer is freed
  bool (*init_write)(void* ctx);                                          // Reset/prepare for writing
  bool (*finish_write)(void* ctx, uint8_t** out_data, size_t* out_size);  // Get write buffer (caller frees)
  bool (*init_read)(void* ctx, const uint8_t* data, size_t size);         // Set up read buffer
  bool (*finish_read)(void* ctx);                                         // Cleanup after reading

  // Write callbacks (return false on error)
  bool (*write_int)(void* ctx, int64_t value);
  bool (*write_uint)(void* ctx, uint64_t value);
  bool (*write_float)(void* ctx, double value);
  bool (*write_string)(void* ctx, const char* value);
  bool (*write_bool)(void* ctx, bool value);
  bool (*write_array_begin)(void* ctx, size_t size);
  bool (*write_array_end)(void* ctx);
  bool (*write_struct_begin)(void* ctx);
  bool (*write_struct_key)(void* ctx, const char* key);
  bool (*write_struct_end)(void* ctx);
  bool (*write_matrix)(void* ctx, size_t ndims, const size_t* dims, const double* data);

  // Read callbacks (return false on error or end-of-container)
  olib_object_type_t (*read_peek)(void* ctx);  // Peek next type without consuming
  bool (*read_int)(void* ctx, int64_t* value);
  bool (*read_uint)(void* ctx, uint64_t* value);
  bool (*read_float)(void* ctx, double* value);
  bool (*read_string)(void* ctx, const char** value);  // Returns pointer, valid until next read
  bool (*read_bool)(void* ctx, bool* value);
  bool (*read_array_begin)(void* ctx, size_t* size);
  bool (*read_array_end)(void* ctx);
  bool (*read_struct_begin)(void* ctx);
  bool (*read_struct_key)(void* ctx, const char** key);  // Returns false when no more keys
  bool (*read_struct_end)(void* ctx);
  bool (*read_matrix)(void* ctx, size_t* ndims, size_t** dims, double** data);  // Caller frees dims/data
} olib_serializer_config_t;

// Serializer management
OLIB_API olib_serializer_t* olib_serializer_new(olib_serializer_config_t* config);
OLIB_API void olib_serializer_free(olib_serializer_t* serializer);

// Check if a serializer is configured as text-based
OLIB_API bool olib_serializer_is_text_based(olib_serializer_t* serializer);

// #############################################################################

// Writing objects
// olib_serializer_write: For binary serializers only (returns false if serializer is text-based)
OLIB_API bool olib_serializer_write(olib_serializer_t* serializer, olib_object_t* obj, uint8_t** out_data, size_t* out_size);

// olib_serializer_write_string: For text-based serializers only (returns false if serializer is binary)
OLIB_API bool olib_serializer_write_string(olib_serializer_t* serializer, olib_object_t* obj, char** out_string);

// olib_serializer_write_file: Works with both text and binary serializers (opens file in appropriate mode)
OLIB_API bool olib_serializer_write_file(olib_serializer_t* serializer, olib_object_t* obj, FILE* file);

// olib_serializer_write_file_path: Works with both text and binary serializers (opens file in appropriate mode)
OLIB_API bool olib_serializer_write_file_path(olib_serializer_t* serializer, olib_object_t* obj, const char* file_path);

// Reading objects
// olib_serializer_read: For binary serializers only (returns NULL if serializer is text-based)
OLIB_API olib_object_t* olib_serializer_read(olib_serializer_t* serializer, const uint8_t* data, size_t size);

// olib_serializer_read_string: For text-based serializers only (returns NULL if serializer is binary)
OLIB_API olib_object_t* olib_serializer_read_string(olib_serializer_t* serializer, const char* string);

// olib_serializer_read_file: Works with both text and binary serializers (reads file in appropriate mode)
OLIB_API olib_object_t* olib_serializer_read_file(olib_serializer_t* serializer, FILE* file);

// olib_serializer_read_file_path: Works with both text and binary serializers (opens file in appropriate mode)
OLIB_API olib_object_t* olib_serializer_read_file_path(olib_serializer_t* serializer, const char* file_path);

// #############################################################################
OLIB_HEADER_END;
// #############################################################################