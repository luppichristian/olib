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

// Thread safety: object operations that allocate or free depend on the allocator
// contract in olib_base.h. Objects have no internal synchronization. Concurrent
// reads are permitted on a stable object tree; synchronize mutations and frees
// with every access to affected objects, descendants, and borrowed pointers.
// Inputs such as keys and strings must remain valid and unmodified while read.
// Numeric getters can parse strings using the C locale; coordinate with changes
// to the process locale.

typedef enum olib_object_type_t {
  OLIB_OBJECT_TYPE_STRUCT,
  OLIB_OBJECT_TYPE_LIST,
  OLIB_OBJECT_TYPE_INT,
  OLIB_OBJECT_TYPE_UINT,
  OLIB_OBJECT_TYPE_FLOAT,
  OLIB_OBJECT_TYPE_STRING,
  OLIB_OBJECT_TYPE_BOOL,
  OLIB_OBJECT_TYPE_MAX,
} olib_object_type_t;

// Thread safety: safe to call concurrently.
OLIB_API const char* olib_object_type_to_string(olib_object_type_t type);

// #############################################################################

typedef struct olib_object_t olib_object_t;

// Object creation and management
// Thread safety: concurrent calls are safe under the allocator contract.
OLIB_API olib_object_t* olib_object_new(olib_object_type_t type);  // Value is empty / zero-initialized
// Thread safety: the source tree must remain stable for the entire copy.
OLIB_API olib_object_t* olib_object_dupe(olib_object_t* obj);
// Thread safety: requires exclusive access to the object and its descendants.
OLIB_API void olib_object_free(olib_object_t* obj);

// Helper getters
// Thread safety: concurrent reads are safe while the accessed object remains stable.
OLIB_API olib_object_type_t olib_object_get_type(olib_object_t* obj);
// Thread safety: concurrent reads are safe while the accessed object remains stable.
OLIB_API bool olib_object_is_type(olib_object_t* obj, olib_object_type_t type);
// Thread safety: concurrent reads are safe while the accessed object remains stable.
OLIB_API bool olib_object_is_value(olib_object_t* obj);
// Thread safety: concurrent reads are safe while the accessed object remains stable.
OLIB_API bool olib_object_is_container(olib_object_t* obj);

// #############################################################################

// List getters
// Thread safety: concurrent reads are safe while the accessed object remains stable.
OLIB_API size_t olib_object_list_size(olib_object_t* obj);
// Thread safety: concurrent reads are safe while the accessed object remains stable.
OLIB_API olib_object_t* olib_object_list_get(olib_object_t* obj, size_t index);

// List setters
// Thread safety: synchronize access to affected objects and any supplied value.
OLIB_API bool olib_object_list_set(olib_object_t* obj, size_t index, olib_object_t* value);
// Thread safety: synchronize access to affected objects and any supplied value.
OLIB_API bool olib_object_list_insert(olib_object_t* obj, size_t index, olib_object_t* value);
// Thread safety: synchronize access to affected objects and any supplied value.
OLIB_API bool olib_object_list_remove(olib_object_t* obj, size_t index);
// Thread safety: synchronize access to affected objects and any supplied value.
OLIB_API bool olib_object_list_push(olib_object_t* obj, olib_object_t* value);
// Thread safety: synchronize access to affected objects and any supplied value.
OLIB_API bool olib_object_list_pop(olib_object_t* obj);

// #############################################################################

// Struct getters
// Thread safety: concurrent reads are safe while the accessed object remains stable.
OLIB_API size_t olib_object_struct_size(olib_object_t* obj);
// Thread safety: concurrent reads are safe while the accessed object remains stable.
OLIB_API bool olib_object_struct_has(olib_object_t* obj, const char* key);
// Thread safety: concurrent reads are safe while the accessed object remains stable.
OLIB_API olib_object_t* olib_object_struct_get(olib_object_t* obj, const char* key);
// Thread safety: concurrent reads are safe while the accessed object remains stable.
OLIB_API const char* olib_object_struct_key_at(olib_object_t* obj, size_t index);
// Thread safety: concurrent reads are safe while the accessed object remains stable.
OLIB_API olib_object_t* olib_object_struct_value_at(olib_object_t* obj, size_t index);

// Struct setters
// Thread safety: synchronize access to affected objects and any supplied value.
OLIB_API bool olib_object_struct_add(olib_object_t* obj, const char* key, olib_object_t* value);  // Fails if key exists
// Thread safety: synchronize access to affected objects and any supplied value.
OLIB_API bool olib_object_struct_set(olib_object_t* obj, const char* key, olib_object_t* value);  // Overwrites existing key, if it does not exist it is created
// Thread safety: synchronize access to affected objects and any supplied value.
OLIB_API bool olib_object_struct_remove(olib_object_t* obj, const char* key);

// #############################################################################

// Numeric and boolean getters convert compatible scalar types; containers return defaults
// String getter only accepts string objects and returns internal storage (do not free)
// Thread safety: concurrent reads are safe while the accessed object remains stable.
OLIB_API int64_t olib_object_get_int(olib_object_t* obj);
// Thread safety: concurrent reads are safe while the accessed object remains stable.
OLIB_API uint64_t olib_object_get_uint(olib_object_t* obj);
// Thread safety: concurrent reads are safe while the accessed object remains stable.
OLIB_API double olib_object_get_float(olib_object_t* obj);
// Thread safety: concurrent reads are safe while the accessed object remains stable.
OLIB_API const char* olib_object_get_string(olib_object_t* obj);  // Returns internal string pointer (valid until object is modified/freed)
// Thread safety: concurrent reads are safe while the accessed object remains stable.
OLIB_API bool olib_object_get_bool(olib_object_t* obj);

// Value setters - set value in the object (must be correct type)
// For string setter: makes internal copy of the string
// Returns false if object is NULL or wrong type
// Thread safety: synchronize access to affected objects and any supplied value.
OLIB_API bool olib_object_set_int(olib_object_t* obj, int64_t value);
// Thread safety: synchronize access to affected objects and any supplied value.
OLIB_API bool olib_object_set_uint(olib_object_t* obj, uint64_t value);
// Thread safety: synchronize access to affected objects and any supplied value.
OLIB_API bool olib_object_set_float(olib_object_t* obj, double value);
// Thread safety: synchronize access to affected objects and any supplied value.
OLIB_API bool olib_object_set_string(olib_object_t* obj, const char* value);  // Makes copy of string
// Thread safety: synchronize access to affected objects and any supplied value.
OLIB_API bool olib_object_set_bool(olib_object_t* obj, bool value);

// #############################################################################
OLIB_HEADER_END;
// #############################################################################
