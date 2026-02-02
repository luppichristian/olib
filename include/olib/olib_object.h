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

#include "olib_base.h"

// #############################################################################
OLIB_HEADER_BEGIN;
// #############################################################################

typedef enum olib_object_type_t {
  OLIB_OBJECT_TYPE_STRUCT,
  OLIB_OBJECT_TYPE_ARRAY,
  OLIB_OBJECT_TYPE_INT,
  OLIB_OBJECT_TYPE_UINT,
  OLIB_OBJECT_TYPE_FLOAT,
  OLIB_OBJECT_TYPE_STRING,
  OLIB_OBJECT_TYPE_BOOL,
  OLIB_OBJECT_TYPE_MATRIX,
  OLIB_OBJECT_TYPE_MAX,
} olib_object_type_t;

OLIB_API const char* olib_object_type_to_string(olib_object_type_t type);

// #############################################################################

typedef struct olib_object_t olib_object_t;

// Object creation and management
OLIB_API olib_object_t* olib_object_new(olib_object_type_t type);  // Value is empty / zero-initialized
OLIB_API olib_object_t* olib_object_dupe(olib_object_t* obj);
OLIB_API void olib_object_free(olib_object_t* obj);

// Helper getters
OLIB_API olib_object_type_t olib_object_get_type(olib_object_t* obj);
OLIB_API bool olib_object_is_type(olib_object_t* obj, olib_object_type_t type);
OLIB_API bool olib_object_is_value(olib_object_t* obj);
OLIB_API bool olib_object_is_container(olib_object_t* obj);

// #############################################################################

// Array getters
OLIB_API size_t olib_object_array_size(olib_object_t* obj);
OLIB_API olib_object_t* olib_object_array_get(olib_object_t* obj, size_t index);

// Array setters
OLIB_API bool olib_object_array_set(olib_object_t* obj, size_t index, olib_object_t* value);
OLIB_API bool olib_object_array_insert(olib_object_t* obj, size_t index, olib_object_t* value);
OLIB_API bool olib_object_array_remove(olib_object_t* obj, size_t index);
OLIB_API bool olib_object_array_push(olib_object_t* obj, olib_object_t* value);
OLIB_API bool olib_object_array_pop(olib_object_t* obj);

// #############################################################################

// Struct getters
OLIB_API size_t olib_object_struct_size(olib_object_t* obj);
OLIB_API bool olib_object_struct_has(olib_object_t* obj, const char* key);
OLIB_API olib_object_t* olib_object_struct_get(olib_object_t* obj, const char* key);
OLIB_API const char* olib_object_struct_key_at(olib_object_t* obj, size_t index);
OLIB_API olib_object_t* olib_object_struct_value_at(olib_object_t* obj, size_t index);

// Struct setters
OLIB_API bool olib_object_struct_add(olib_object_t* obj, const char* key, olib_object_t* value);  // Fails if key exists
OLIB_API bool olib_object_struct_set(olib_object_t* obj, const char* key, olib_object_t* value);  // Overwrites existing key, if it does not exist it is created
OLIB_API bool olib_object_struct_remove(olib_object_t* obj, const char* key);

// #############################################################################

// Value getters - return value stored in the object
// For string getter: returns pointer to internal null-terminated string (do not free)
// Returns appropriate default values if object is NULL or wrong type
OLIB_API int64_t olib_object_get_int(olib_object_t* obj);
OLIB_API uint64_t olib_object_get_uint(olib_object_t* obj);
OLIB_API double olib_object_get_float(olib_object_t* obj);
OLIB_API const char* olib_object_get_string(olib_object_t* obj);  // Returns internal string pointer (valid until object is modified/freed)
OLIB_API bool olib_object_get_bool(olib_object_t* obj);

// Value setters - set value in the object (must be correct type)
// For string setter: makes internal copy of the string
// Returns false if object is NULL or wrong type
OLIB_API bool olib_object_set_int(olib_object_t* obj, int64_t value);
OLIB_API bool olib_object_set_uint(olib_object_t* obj, uint64_t value);
OLIB_API bool olib_object_set_float(olib_object_t* obj, double value);
OLIB_API bool olib_object_set_string(olib_object_t* obj, const char* value);  // Makes copy of string
OLIB_API bool olib_object_set_bool(olib_object_t* obj, bool value);

// #############################################################################

// Matrix creation (use this instead of olib_object_new for matrices)
OLIB_API olib_object_t* olib_object_matrix_new(size_t ndims, const size_t* dims);

// Matrix getters
OLIB_API size_t olib_object_matrix_ndims(olib_object_t* obj);
OLIB_API size_t olib_object_matrix_dim(olib_object_t* obj, size_t axis);
OLIB_API const size_t* olib_object_matrix_dims(olib_object_t* obj);
OLIB_API size_t olib_object_matrix_total_size(olib_object_t* obj);
OLIB_API double olib_object_matrix_get(olib_object_t* obj, const size_t* indices);
OLIB_API double* olib_object_matrix_data(olib_object_t* obj);

// Matrix setters
OLIB_API bool olib_object_matrix_set(olib_object_t* obj, const size_t* indices, double value);
OLIB_API bool olib_object_matrix_fill(olib_object_t* obj, double value);
OLIB_API bool olib_object_matrix_set_data(olib_object_t* obj, const double* data, size_t count);

// #############################################################################
OLIB_HEADER_END;
// #############################################################################