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

#include <olib/olib_serializer.h>
#include <string.h>

// #############################################################################
// Internal structures
// #############################################################################

struct olib_serializer_t {
    olib_serializer_config_t config;
};

// #############################################################################
// Serializer management
// #############################################################################

OLIB_API olib_serializer_t* olib_serializer_new(olib_serializer_config_t* config) {
    if (!config) {
        return NULL;
    }

    olib_serializer_t* serializer = olib_calloc(1, sizeof(olib_serializer_t));
    if (!serializer) {
        return NULL;
    }

    serializer->config = *config;
    return serializer;
}

OLIB_API void olib_serializer_free(olib_serializer_t* serializer) {
    if (!serializer) {
        return;
    }
    olib_free(serializer);
}

// #############################################################################
// Internal write helpers
// #############################################################################

static bool olib_serializer_write_object(olib_serializer_t* serializer, olib_object_t* obj) {
    if (!serializer || !obj) {
        return false;
    }

    olib_serializer_config_t* cfg = &serializer->config;
    void* ctx = cfg->user_data;

    switch (olib_object_get_type(obj)) {
        case OLIB_OBJECT_TYPE_INT:
            if (!cfg->write_int) return false;
            return cfg->write_int(ctx, olib_object_get_int(obj));

        case OLIB_OBJECT_TYPE_UINT:
            if (!cfg->write_uint) return false;
            return cfg->write_uint(ctx, olib_object_get_uint(obj));

        case OLIB_OBJECT_TYPE_FLOAT:
            if (!cfg->write_float) return false;
            return cfg->write_float(ctx, olib_object_get_float(obj));

        case OLIB_OBJECT_TYPE_STRING:
            if (!cfg->write_string) return false;
            return cfg->write_string(ctx, olib_object_get_string(obj));

        case OLIB_OBJECT_TYPE_BOOL:
            if (!cfg->write_bool) return false;
            return cfg->write_bool(ctx, olib_object_get_bool(obj));

        case OLIB_OBJECT_TYPE_ARRAY: {
            if (!cfg->write_array_begin || !cfg->write_array_end) return false;
            size_t size = olib_object_array_size(obj);
            if (!cfg->write_array_begin(ctx, size)) return false;
            for (size_t i = 0; i < size; i++) {
                olib_object_t* item = olib_object_array_get(obj, i);
                if (!olib_serializer_write_object(serializer, item)) return false;
            }
            return cfg->write_array_end(ctx);
        }

        case OLIB_OBJECT_TYPE_STRUCT: {
            if (!cfg->write_struct_begin || !cfg->write_struct_key || !cfg->write_struct_end) return false;
            if (!cfg->write_struct_begin(ctx)) return false;
            size_t size = olib_object_struct_size(obj);
            for (size_t i = 0; i < size; i++) {
                const char* key = olib_object_struct_key_at(obj, i);
                olib_object_t* value = olib_object_struct_value_at(obj, i);
                if (!cfg->write_struct_key(ctx, key)) return false;
                if (!olib_serializer_write_object(serializer, value)) return false;
            }
            return cfg->write_struct_end(ctx);
        }

        case OLIB_OBJECT_TYPE_MATRIX: {
            if (!cfg->write_matrix) return false;
            size_t ndims = olib_object_matrix_ndims(obj);
            const size_t* dims = olib_object_matrix_dims(obj);
            const double* data = olib_object_matrix_data(obj);
            return cfg->write_matrix(ctx, ndims, dims, data);
        }

        default:
            return false;
    }
}

// #############################################################################
// Internal read helpers
// #############################################################################

static olib_object_t* olib_serializer_read_object(olib_serializer_t* serializer) {
    if (!serializer) {
        return NULL;
    }

    olib_serializer_config_t* cfg = &serializer->config;
    void* ctx = cfg->user_data;

    if (!cfg->read_peek) {
        return NULL;
    }

    olib_object_type_t type = cfg->read_peek(ctx);
    olib_object_t* obj = NULL;

    switch (type) {
        case OLIB_OBJECT_TYPE_INT: {
            if (!cfg->read_int) return NULL;
            int64_t value;
            if (!cfg->read_int(ctx, &value)) return NULL;
            obj = olib_object_new(OLIB_OBJECT_TYPE_INT);
            if (!obj) return NULL;
            olib_object_set_int(obj, value);
            return obj;
        }

        case OLIB_OBJECT_TYPE_UINT: {
            if (!cfg->read_uint) return NULL;
            uint64_t value;
            if (!cfg->read_uint(ctx, &value)) return NULL;
            obj = olib_object_new(OLIB_OBJECT_TYPE_UINT);
            if (!obj) return NULL;
            olib_object_set_uint(obj, value);
            return obj;
        }

        case OLIB_OBJECT_TYPE_FLOAT: {
            if (!cfg->read_float) return NULL;
            double value;
            if (!cfg->read_float(ctx, &value)) return NULL;
            obj = olib_object_new(OLIB_OBJECT_TYPE_FLOAT);
            if (!obj) return NULL;
            olib_object_set_float(obj, value);
            return obj;
        }

        case OLIB_OBJECT_TYPE_STRING: {
            if (!cfg->read_string) return NULL;
            const char* value;
            if (!cfg->read_string(ctx, &value)) return NULL;
            obj = olib_object_new(OLIB_OBJECT_TYPE_STRING);
            if (!obj) return NULL;
            olib_object_set_string(obj, value);
            return obj;
        }

        case OLIB_OBJECT_TYPE_BOOL: {
            if (!cfg->read_bool) return NULL;
            bool value;
            if (!cfg->read_bool(ctx, &value)) return NULL;
            obj = olib_object_new(OLIB_OBJECT_TYPE_BOOL);
            if (!obj) return NULL;
            olib_object_set_bool(obj, value);
            return obj;
        }

        case OLIB_OBJECT_TYPE_ARRAY: {
            if (!cfg->read_array_begin || !cfg->read_array_end) return NULL;
            size_t size;
            if (!cfg->read_array_begin(ctx, &size)) return NULL;
            obj = olib_object_new(OLIB_OBJECT_TYPE_ARRAY);
            if (!obj) return NULL;
            for (size_t i = 0; i < size; i++) {
                olib_object_t* item = olib_serializer_read_object(serializer);
                if (!item) {
                    olib_object_free(obj);
                    return NULL;
                }
                if (!olib_object_array_push(obj, item)) {
                    olib_object_free(item);
                    olib_object_free(obj);
                    return NULL;
                }
            }
            if (!cfg->read_array_end(ctx)) {
                olib_object_free(obj);
                return NULL;
            }
            return obj;
        }

        case OLIB_OBJECT_TYPE_STRUCT: {
            if (!cfg->read_struct_begin || !cfg->read_struct_key || !cfg->read_struct_end) return NULL;
            if (!cfg->read_struct_begin(ctx)) return NULL;
            obj = olib_object_new(OLIB_OBJECT_TYPE_STRUCT);
            if (!obj) return NULL;
            const char* key;
            while (cfg->read_struct_key(ctx, &key)) {
                olib_object_t* value = olib_serializer_read_object(serializer);
                if (!value) {
                    olib_object_free(obj);
                    return NULL;
                }
                if (!olib_object_struct_set(obj, key, value)) {
                    olib_object_free(value);
                    olib_object_free(obj);
                    return NULL;
                }
            }
            if (!cfg->read_struct_end(ctx)) {
                olib_object_free(obj);
                return NULL;
            }
            return obj;
        }

        case OLIB_OBJECT_TYPE_MATRIX: {
            if (!cfg->read_matrix) return NULL;
            size_t ndims;
            size_t* dims;
            double* data;
            if (!cfg->read_matrix(ctx, &ndims, &dims, &data)) return NULL;
            obj = olib_object_matrix_new(ndims, dims);
            if (!obj) {
                olib_free(dims);
                olib_free(data);
                return NULL;
            }
            size_t total = olib_object_matrix_total_size(obj);
            olib_object_matrix_set_data(obj, data, total);
            olib_free(dims);
            olib_free(data);
            return obj;
        }

        default:
            return NULL;
    }
}

// #############################################################################
// Public write functions
// #############################################################################

OLIB_API bool olib_serializer_write(olib_serializer_t* serializer, olib_object_t* obj, uint8_t** out_data, size_t* out_size) {
    if (!serializer || !obj) {
        return false;
    }
    // The callbacks are responsible for writing to a buffer accessible via user_data
    // This function just triggers the serialization
    // The user must set up user_data to collect the output
    return olib_serializer_write_object(serializer, obj);
}

OLIB_API bool olib_serializer_write_string(olib_serializer_t* serializer, olib_object_t* obj, char** out_string) {
    if (!serializer || !obj) {
        return false;
    }
    return olib_serializer_write_object(serializer, obj);
}

OLIB_API bool olib_serializer_write_file(olib_serializer_t* serializer, olib_object_t* obj, FILE* file) {
    if (!serializer || !obj || !file) {
        return false;
    }
    return olib_serializer_write_object(serializer, obj);
}

OLIB_API bool olib_serializer_write_file_path(olib_serializer_t* serializer, olib_object_t* obj, const char* file_path) {
    if (!serializer || !obj || !file_path) {
        return false;
    }
    FILE* file = fopen(file_path, "wb");
    if (!file) {
        return false;
    }
    bool result = olib_serializer_write_file(serializer, obj, file);
    fclose(file);
    return result;
}

// #############################################################################
// Public read functions
// #############################################################################

OLIB_API olib_object_t* olib_serializer_read(olib_serializer_t* serializer, const uint8_t* data, size_t size) {
    if (!serializer || !data || size == 0) {
        return NULL;
    }
    // The callbacks are responsible for reading from a buffer accessible via user_data
    // The user must set up user_data with the input data before calling
    return olib_serializer_read_object(serializer);
}

OLIB_API olib_object_t* olib_serializer_read_string(olib_serializer_t* serializer, const char* string) {
    if (!serializer || !string) {
        return NULL;
    }
    return olib_serializer_read_object(serializer);
}

OLIB_API olib_object_t* olib_serializer_read_file(olib_serializer_t* serializer, FILE* file) {
    if (!serializer || !file) {
        return NULL;
    }
    return olib_serializer_read_object(serializer);
}

OLIB_API olib_object_t* olib_serializer_read_file_path(olib_serializer_t* serializer, const char* file_path) {
    if (!serializer || !file_path) {
        return NULL;
    }
    FILE* file = fopen(file_path, "rb");
    if (!file) {
        return NULL;
    }
    olib_object_t* result = olib_serializer_read_file(serializer, file);
    fclose(file);
    return result;
}
