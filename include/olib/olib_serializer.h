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
  // TODO: Add configuration callbacks
} olib_serializer_config_t;

// Serializer management
OLIB_API olib_serializer_t* olib_serializer_new(olib_serializer_config_t* config);
OLIB_API void olib_serializer_free(olib_serializer_t* serializer);

// #############################################################################
// Writing objects
OLIB_API bool olib_serializer_write(olib_serializer_t* serializer, olib_object_t* obj, uint8_t** out_data, size_t* out_size);
OLIB_API bool olib_serializer_write_string(olib_serializer_t* serializer, olib_object_t* obj, char** out_string);
OLIB_API bool olib_serializer_write_file(olib_serializer_t* serializer, olib_object_t* obj, FILE* file);
OLIB_API bool olib_serializer_write_file_path(olib_serializer_t* serializer, olib_object_t* obj, const char* file_path);

// Reading objects
OLIB_API olib_object_t* olib_serializer_read(olib_serializer_t* serializer, const uint8_t* data, size_t size);
OLIB_API olib_object_t* olib_serializer_read_string(olib_serializer_t* serializer, const char* string);
OLIB_API olib_object_t* olib_serializer_read_file(olib_serializer_t* serializer, FILE* file);
OLIB_API olib_object_t* olib_serializer_read_file_path(olib_serializer_t* serializer, const char* file_path);

// #############################################################################
OLIB_HEADER_END;
// #############################################################################