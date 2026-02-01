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

#include "olib_serializer.h"

// #############################################################################
OLIB_HEADER_BEGIN;
// #############################################################################

// Default serializer types, these are already implemented in the library,
// but you can of course implement your own serializers as well.
// Use olib_serializer_new_* functions to create new instances.

typedef enum olib_format_t {
  OLIB_FORMAT_JSON_TEXT,
  OLIB_FORMAT_JSON_BINARY,
  OLIB_FORMAT_YAML,
  OLIB_FORMAT_XML,
  OLIB_FORMAT_BINARY,
  OLIB_FORMAT_TOML,
  OLIB_FORMAT_TXT,
  OLIB_FORMAT_MAX,
} olib_format_t;

OLIB_API olib_serializer_t* olib_serializer_new_json_text();
OLIB_API olib_serializer_t* olib_serializer_new_json_binary();
OLIB_API olib_serializer_t* olib_serializer_new_yaml();
OLIB_API olib_serializer_t* olib_serializer_new_xml();
OLIB_API olib_serializer_t* olib_serializer_new_binary();
OLIB_API olib_serializer_t* olib_serializer_new_toml();
OLIB_API olib_serializer_t* olib_serializer_new_txt();

// #############################################################################
OLIB_HEADER_END;
// #############################################################################