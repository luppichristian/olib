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

#include <olib/olib_helpers.h>

// #############################################################################
// Format to serializer mapping
// #############################################################################

OLIB_API olib_serializer_t* olib_format_serializer(olib_format_t format) {
    switch (format) {
        case OLIB_FORMAT_JSON_TEXT:   return olib_serializer_new_json_text();
        case OLIB_FORMAT_JSON_BINARY: return olib_serializer_new_json_binary();
        case OLIB_FORMAT_YAML:        return olib_serializer_new_yaml();
        case OLIB_FORMAT_XML:         return olib_serializer_new_xml();
        case OLIB_FORMAT_BINARY:      return olib_serializer_new_binary();
        case OLIB_FORMAT_TOML:        return olib_serializer_new_toml();
        case OLIB_FORMAT_TXT:         return olib_serializer_new_txt();
        default:                      return NULL;
    }
}

// #############################################################################
// Write helpers
// #############################################################################

OLIB_API bool olib_format_write(olib_format_t format, olib_object_t* obj, uint8_t** out_data, size_t* out_size) {
    if (!obj || !out_data || !out_size) {
        return false;
    }

    olib_serializer_t* serializer = olib_format_serializer(format);
    if (!serializer) {
        return false;
    }

    bool result = olib_serializer_write(serializer, obj, out_data, out_size);
    olib_serializer_free(serializer);
    return result;
}

OLIB_API bool olib_format_write_string(olib_format_t format, olib_object_t* obj, char** out_string) {
    if (!obj || !out_string) {
        return false;
    }

    olib_serializer_t* serializer = olib_format_serializer(format);
    if (!serializer) {
        return false;
    }

    bool result = olib_serializer_write_string(serializer, obj, out_string);
    olib_serializer_free(serializer);
    return result;
}

OLIB_API bool olib_format_write_file(olib_format_t format, olib_object_t* obj, FILE* file) {
    if (!obj || !file) {
        return false;
    }

    olib_serializer_t* serializer = olib_format_serializer(format);
    if (!serializer) {
        return false;
    }

    bool result = olib_serializer_write_file(serializer, obj, file);
    olib_serializer_free(serializer);
    return result;
}

OLIB_API bool olib_format_write_file_path(olib_format_t format, olib_object_t* obj, const char* file_path) {
    if (!obj || !file_path) {
        return false;
    }

    olib_serializer_t* serializer = olib_format_serializer(format);
    if (!serializer) {
        return false;
    }

    bool result = olib_serializer_write_file_path(serializer, obj, file_path);
    olib_serializer_free(serializer);
    return result;
}

// #############################################################################
// Read helpers
// #############################################################################

OLIB_API olib_object_t* olib_format_read(olib_format_t format, const uint8_t* data, size_t size) {
    if (!data || size == 0) {
        return NULL;
    }

    olib_serializer_t* serializer = olib_format_serializer(format);
    if (!serializer) {
        return NULL;
    }

    olib_object_t* result = olib_serializer_read(serializer, data, size);
    olib_serializer_free(serializer);
    return result;
}

OLIB_API olib_object_t* olib_format_read_string(olib_format_t format, const char* string) {
    if (!string) {
        return NULL;
    }

    olib_serializer_t* serializer = olib_format_serializer(format);
    if (!serializer) {
        return NULL;
    }

    olib_object_t* result = olib_serializer_read_string(serializer, string);
    olib_serializer_free(serializer);
    return result;
}

OLIB_API olib_object_t* olib_format_read_file(olib_format_t format, FILE* file) {
    if (!file) {
        return NULL;
    }

    olib_serializer_t* serializer = olib_format_serializer(format);
    if (!serializer) {
        return NULL;
    }

    olib_object_t* result = olib_serializer_read_file(serializer, file);
    olib_serializer_free(serializer);
    return result;
}

OLIB_API olib_object_t* olib_format_read_file_path(olib_format_t format, const char* file_path) {
    if (!file_path) {
        return NULL;
    }

    olib_serializer_t* serializer = olib_format_serializer(format);
    if (!serializer) {
        return NULL;
    }

    olib_object_t* result = olib_serializer_read_file_path(serializer, file_path);
    olib_serializer_free(serializer);
    return result;
}

// #############################################################################
// Conversion helpers
// #############################################################################

OLIB_API bool olib_convert(
    olib_format_t src_format, const uint8_t* src_data, size_t src_size,
    olib_format_t dst_format, uint8_t** out_data, size_t* out_size)
{
    if (!src_data || src_size == 0 || !out_data || !out_size) {
        return false;
    }

    // Read from source format
    olib_object_t* obj = olib_format_read(src_format, src_data, src_size);
    if (!obj) {
        return false;
    }

    // Write to destination format
    bool result = olib_format_write(dst_format, obj, out_data, out_size);
    olib_object_free(obj);
    return result;
}

OLIB_API bool olib_convert_string(
    olib_format_t src_format, const char* src_string,
    olib_format_t dst_format, char** out_string)
{
    if (!src_string || !out_string) {
        return false;
    }

    // Read from source format
    olib_object_t* obj = olib_format_read_string(src_format, src_string);
    if (!obj) {
        return false;
    }

    // Write to destination format
    bool result = olib_format_write_string(dst_format, obj, out_string);
    olib_object_free(obj);
    return result;
}

OLIB_API bool olib_convert_file(
    olib_format_t src_format, FILE* src_file,
    olib_format_t dst_format, FILE* dst_file)
{
    if (!src_file || !dst_file) {
        return false;
    }

    // Read from source format
    olib_object_t* obj = olib_format_read_file(src_format, src_file);
    if (!obj) {
        return false;
    }

    // Write to destination format
    bool result = olib_format_write_file(dst_format, obj, dst_file);
    olib_object_free(obj);
    return result;
}

OLIB_API bool olib_convert_file_path(
    olib_format_t src_format, const char* src_path,
    olib_format_t dst_format, const char* dst_path)
{
    if (!src_path || !dst_path) {
        return false;
    }

    // Read from source format
    olib_object_t* obj = olib_format_read_file_path(src_format, src_path);
    if (!obj) {
        return false;
    }

    // Write to destination format
    bool result = olib_format_write_file_path(dst_format, obj, dst_path);
    olib_object_free(obj);
    return result;
}
