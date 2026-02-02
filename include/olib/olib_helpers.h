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

#include "olib_formats.h"

// #############################################################################
OLIB_HEADER_BEGIN;
// #############################################################################

// Get a new serializer for a given format (caller must free with olib_serializer_free)
OLIB_API olib_serializer_t* olib_format_serializer(olib_format_t format);

// #############################################################################
// Write helpers - write object to various outputs using a format
// #############################################################################

// Write object to memory buffer (caller must free out_data with olib_free)
OLIB_API bool olib_format_write(olib_format_t format, olib_object_t* obj, uint8_t** out_data, size_t* out_size);

// Write object to null-terminated string (caller must free out_string with olib_free)
// Only use with text-based formats (JSON_TEXT, YAML, XML, TOML, TXT)
// For binary formats, use olib_format_write() instead
OLIB_API bool olib_format_write_string(olib_format_t format, olib_object_t* obj, char** out_string);

// Write object to FILE*
OLIB_API bool olib_format_write_file(olib_format_t format, olib_object_t* obj, FILE* file);

// Write object to file path
OLIB_API bool olib_format_write_file_path(olib_format_t format, olib_object_t* obj, const char* file_path);

// #############################################################################
// Read helpers - read object from various inputs using a format
// #############################################################################

// Read object from memory buffer (caller must free returned object with olib_object_free)
OLIB_API olib_object_t* olib_format_read(olib_format_t format, const uint8_t* data, size_t size);

// Read object from null-terminated string (caller must free returned object with olib_object_free)
// Only use with text-based formats (JSON_TEXT, YAML, XML, TOML, TXT)
// For binary formats, use olib_format_read() instead
OLIB_API olib_object_t* olib_format_read_string(olib_format_t format, const char* string);

// Read object from FILE* (caller must free returned object with olib_object_free)
OLIB_API olib_object_t* olib_format_read_file(olib_format_t format, FILE* file);

// Read object from file path (caller must free returned object with olib_object_free)
OLIB_API olib_object_t* olib_format_read_file_path(olib_format_t format, const char* file_path);

// #############################################################################
// Conversion helpers - convert between formats directly
// #############################################################################

// Convert memory buffer from one format to another (caller must free out_data with olib_free)
OLIB_API bool olib_convert(
    olib_format_t src_format,
    const uint8_t* src_data,
    size_t src_size,
    olib_format_t dst_format,
    uint8_t** out_data,
    size_t* out_size);

// Convert between text-based formats (caller must free out_string with olib_free)
// Both src_format and dst_format must be text-based (JSON_TEXT, YAML, XML, TOML, TXT)
// For binary source data, use olib_convert() which accepts size parameter
// For binary destination, use olib_convert() which provides size in output
OLIB_API bool olib_convert_string(
    olib_format_t src_format,
    const char* src_string,
    olib_format_t dst_format,
    char** out_string);

// Convert file from one format to another
OLIB_API bool olib_convert_file(
    olib_format_t src_format,
    FILE* src_file,
    olib_format_t dst_format,
    FILE* dst_file);

// Convert file path from one format to another
OLIB_API bool olib_convert_file_path(
    olib_format_t src_format,
    const char* src_path,
    olib_format_t dst_format,
    const char* dst_path);

// #############################################################################
OLIB_HEADER_END;
// #############################################################################
