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

#include <olib/olib_object.h>
#include <string.h>

// #############################################################################
// Internal structures
// #############################################################################

typedef struct olib_struct_entry_t {
    char* key;
    olib_object_t* value;
} olib_struct_entry_t;

struct olib_object_t {
    olib_object_type_t type;
    union {
        // Value types
        int64_t int_val;
        uint64_t uint_val;
        double float_val;
        char* string_val;
        bool bool_val;
        // List type
        struct {
            olib_object_t** items;
            size_t size;
            size_t capacity;
        } list;
        // Struct type
        struct {
            olib_struct_entry_t* entries;
            size_t size;
            size_t capacity;
        } object;
        // Matrix type
        struct {
            double* data;
            size_t* dims;
            size_t ndims;
            size_t total_size;
        } matrix;
    } data;
};

// #############################################################################
// Type to string
// #############################################################################

static const char* g_type_strings[OLIB_OBJECT_TYPE_MAX] = {
    "struct",
    "list",
    "int",
    "uint",
    "float",
    "string",
    "bool",
    "matrix",
};

OLIB_API const char* olib_object_type_to_string(olib_object_type_t type) {
    if (type >= 0 && type < OLIB_OBJECT_TYPE_MAX) {
        return g_type_strings[type];
    }
    return "unknown";
}

// #############################################################################
// Object creation and management
// #############################################################################

OLIB_API olib_object_t* olib_object_new(olib_object_type_t type) {
    olib_object_t* obj = olib_calloc(1, sizeof(olib_object_t));
    if (!obj) {
        return NULL;
    }
    obj->type = type;
    return obj;
}

OLIB_API olib_object_t* olib_object_dupe(olib_object_t* obj) {
    if (!obj) {
        return NULL;
    }

    olib_object_t* copy = olib_object_new(obj->type);
    if (!copy) {
        return NULL;
    }

    switch (obj->type) {
        case OLIB_OBJECT_TYPE_INT:
            copy->data.int_val = obj->data.int_val;
            break;
        case OLIB_OBJECT_TYPE_UINT:
            copy->data.uint_val = obj->data.uint_val;
            break;
        case OLIB_OBJECT_TYPE_FLOAT:
            copy->data.float_val = obj->data.float_val;
            break;
        case OLIB_OBJECT_TYPE_BOOL:
            copy->data.bool_val = obj->data.bool_val;
            break;
        case OLIB_OBJECT_TYPE_STRING:
            if (obj->data.string_val) {
                size_t len = strlen(obj->data.string_val);
                copy->data.string_val = olib_malloc(len + 1);
                if (!copy->data.string_val) {
                    olib_object_free(copy);
                    return NULL;
                }
                memcpy(copy->data.string_val, obj->data.string_val, len + 1);
            }
            break;
        case OLIB_OBJECT_TYPE_LIST:
            if (obj->data.list.size > 0) {
                copy->data.list.items = olib_malloc(obj->data.list.size * sizeof(olib_object_t*));
                if (!copy->data.list.items) {
                    olib_object_free(copy);
                    return NULL;
                }
                copy->data.list.capacity = obj->data.list.size;
                for (size_t i = 0; i < obj->data.list.size; i++) {
                    copy->data.list.items[i] = olib_object_dupe(obj->data.list.items[i]);
                    if (obj->data.list.items[i] && !copy->data.list.items[i]) {
                        olib_object_free(copy);
                        return NULL;
                    }
                    copy->data.list.size++;
                }
            }
            break;
        case OLIB_OBJECT_TYPE_STRUCT:
            if (obj->data.object.size > 0) {
                copy->data.object.entries = olib_malloc(obj->data.object.size * sizeof(olib_struct_entry_t));
                if (!copy->data.object.entries) {
                    olib_object_free(copy);
                    return NULL;
                }
                copy->data.object.capacity = obj->data.object.size;
                for (size_t i = 0; i < obj->data.object.size; i++) {
                    size_t key_len = strlen(obj->data.object.entries[i].key);
                    copy->data.object.entries[i].key = olib_malloc(key_len + 1);
                    if (!copy->data.object.entries[i].key) {
                        olib_object_free(copy);
                        return NULL;
                    }
                    memcpy(copy->data.object.entries[i].key, obj->data.object.entries[i].key, key_len + 1);
                    copy->data.object.entries[i].value = olib_object_dupe(obj->data.object.entries[i].value);
                    if (obj->data.object.entries[i].value && !copy->data.object.entries[i].value) {
                        olib_object_free(copy);
                        return NULL;
                    }
                    copy->data.object.size++;
                }
            }
            break;
        case OLIB_OBJECT_TYPE_MATRIX:
            copy->data.matrix.ndims = obj->data.matrix.ndims;
            copy->data.matrix.total_size = obj->data.matrix.total_size;
            if (obj->data.matrix.ndims > 0) {
                copy->data.matrix.dims = olib_malloc(obj->data.matrix.ndims * sizeof(size_t));
                if (!copy->data.matrix.dims) {
                    olib_object_free(copy);
                    return NULL;
                }
                memcpy(copy->data.matrix.dims, obj->data.matrix.dims, obj->data.matrix.ndims * sizeof(size_t));
            }
            if (obj->data.matrix.total_size > 0) {
                copy->data.matrix.data = olib_malloc(obj->data.matrix.total_size * sizeof(double));
                if (!copy->data.matrix.data) {
                    olib_object_free(copy);
                    return NULL;
                }
                memcpy(copy->data.matrix.data, obj->data.matrix.data, obj->data.matrix.total_size * sizeof(double));
            }
            break;
        default:
            break;
    }

    return copy;
}

OLIB_API void olib_object_free(olib_object_t* obj) {
    if (!obj) {
        return;
    }

    switch (obj->type) {
        case OLIB_OBJECT_TYPE_STRING:
            if (obj->data.string_val) {
                olib_free(obj->data.string_val);
            }
            break;
        case OLIB_OBJECT_TYPE_LIST:
            for (size_t i = 0; i < obj->data.list.size; i++) {
                olib_object_free(obj->data.list.items[i]);
            }
            if (obj->data.list.items) {
                olib_free(obj->data.list.items);
            }
            break;
        case OLIB_OBJECT_TYPE_STRUCT:
            for (size_t i = 0; i < obj->data.object.size; i++) {
                olib_free(obj->data.object.entries[i].key);
                olib_object_free(obj->data.object.entries[i].value);
            }
            if (obj->data.object.entries) {
                olib_free(obj->data.object.entries);
            }
            break;
        case OLIB_OBJECT_TYPE_MATRIX:
            if (obj->data.matrix.dims) {
                olib_free(obj->data.matrix.dims);
            }
            if (obj->data.matrix.data) {
                olib_free(obj->data.matrix.data);
            }
            break;
        default:
            break;
    }

    olib_free(obj);
}

// #############################################################################
// Helper getters
// #############################################################################

OLIB_API olib_object_type_t olib_object_get_type(olib_object_t* obj) {
    if (!obj) {
        return OLIB_OBJECT_TYPE_MAX;
    }
    return obj->type;
}

OLIB_API bool olib_object_is_type(olib_object_t* obj, olib_object_type_t type) {
    if (!obj) {
        return false;
    }
    return obj->type == type;
}

OLIB_API bool olib_object_is_value(olib_object_t* obj) {
    if (!obj) {
        return false;
    }
    return obj->type == OLIB_OBJECT_TYPE_INT ||
           obj->type == OLIB_OBJECT_TYPE_UINT ||
           obj->type == OLIB_OBJECT_TYPE_FLOAT ||
           obj->type == OLIB_OBJECT_TYPE_STRING ||
           obj->type == OLIB_OBJECT_TYPE_BOOL;
}

OLIB_API bool olib_object_is_container(olib_object_t* obj) {
    if (!obj) {
        return false;
    }
    return obj->type == OLIB_OBJECT_TYPE_STRUCT ||
           obj->type == OLIB_OBJECT_TYPE_LIST;
}

// #############################################################################
// List operations
// #############################################################################

OLIB_API size_t olib_object_list_size(olib_object_t* obj) {
    if (!obj || obj->type != OLIB_OBJECT_TYPE_LIST) {
        return 0;
    }
    return obj->data.list.size;
}

OLIB_API olib_object_t* olib_object_list_get(olib_object_t* obj, size_t index) {
    if (!obj || obj->type != OLIB_OBJECT_TYPE_LIST) {
        return NULL;
    }
    if (index >= obj->data.list.size) {
        return NULL;
    }
    return obj->data.list.items[index];
}

static bool olib_object_list_grow(olib_object_t* obj, size_t min_capacity) {
    if (obj->data.list.capacity >= min_capacity) {
        return true;
    }
    size_t new_capacity = obj->data.list.capacity ? obj->data.list.capacity * 2 : 4;
    while (new_capacity < min_capacity) {
        new_capacity *= 2;
    }
    olib_object_t** new_items = olib_realloc(obj->data.list.items, new_capacity * sizeof(olib_object_t*));
    if (!new_items) {
        return false;
    }
    obj->data.list.items = new_items;
    obj->data.list.capacity = new_capacity;
    return true;
}

OLIB_API bool olib_object_list_set(olib_object_t* obj, size_t index, olib_object_t* value) {
    if (!obj || obj->type != OLIB_OBJECT_TYPE_LIST) {
        return false;
    }
    if (index >= obj->data.list.size) {
        return false;
    }
    olib_object_free(obj->data.list.items[index]);
    obj->data.list.items[index] = value;
    return true;
}

OLIB_API bool olib_object_list_insert(olib_object_t* obj, size_t index, olib_object_t* value) {
    if (!obj || obj->type != OLIB_OBJECT_TYPE_LIST) {
        return false;
    }
    if (index > obj->data.list.size) {
        return false;
    }
    if (!olib_object_list_grow(obj, obj->data.list.size + 1)) {
        return false;
    }
    for (size_t i = obj->data.list.size; i > index; i--) {
        obj->data.list.items[i] = obj->data.list.items[i - 1];
    }
    obj->data.list.items[index] = value;
    obj->data.list.size++;
    return true;
}

OLIB_API bool olib_object_list_remove(olib_object_t* obj, size_t index) {
    if (!obj || obj->type != OLIB_OBJECT_TYPE_LIST) {
        return false;
    }
    if (index >= obj->data.list.size) {
        return false;
    }
    olib_object_free(obj->data.list.items[index]);
    for (size_t i = index; i < obj->data.list.size - 1; i++) {
        obj->data.list.items[i] = obj->data.list.items[i + 1];
    }
    obj->data.list.size--;
    return true;
}

OLIB_API bool olib_object_list_push(olib_object_t* obj, olib_object_t* value) {
    if (!obj || obj->type != OLIB_OBJECT_TYPE_LIST) {
        return false;
    }
    return olib_object_list_insert(obj, obj->data.list.size, value);
}

OLIB_API bool olib_object_list_pop(olib_object_t* obj) {
    if (!obj || obj->type != OLIB_OBJECT_TYPE_LIST) {
        return false;
    }
    if (obj->data.list.size == 0) {
        return false;
    }
    return olib_object_list_remove(obj, obj->data.list.size - 1);
}

// #############################################################################
// Struct operations
// #############################################################################

OLIB_API size_t olib_object_struct_size(olib_object_t* obj) {
    if (!obj || obj->type != OLIB_OBJECT_TYPE_STRUCT) {
        return 0;
    }
    return obj->data.object.size;
}

static olib_struct_entry_t* olib_object_struct_find(olib_object_t* obj, const char* key) {
    for (size_t i = 0; i < obj->data.object.size; i++) {
        if (strcmp(obj->data.object.entries[i].key, key) == 0) {
            return &obj->data.object.entries[i];
        }
    }
    return NULL;
}

OLIB_API bool olib_object_struct_has(olib_object_t* obj, const char* key) {
    if (!obj || obj->type != OLIB_OBJECT_TYPE_STRUCT || !key) {
        return false;
    }
    return olib_object_struct_find(obj, key) != NULL;
}

OLIB_API olib_object_t* olib_object_struct_get(olib_object_t* obj, const char* key) {
    if (!obj || obj->type != OLIB_OBJECT_TYPE_STRUCT || !key) {
        return NULL;
    }
    olib_struct_entry_t* entry = olib_object_struct_find(obj, key);
    return entry ? entry->value : NULL;
}

OLIB_API const char* olib_object_struct_key_at(olib_object_t* obj, size_t index) {
    if (!obj || obj->type != OLIB_OBJECT_TYPE_STRUCT) {
        return NULL;
    }
    if (index >= obj->data.object.size) {
        return NULL;
    }
    return obj->data.object.entries[index].key;
}

OLIB_API olib_object_t* olib_object_struct_value_at(olib_object_t* obj, size_t index) {
    if (!obj || obj->type != OLIB_OBJECT_TYPE_STRUCT) {
        return NULL;
    }
    if (index >= obj->data.object.size) {
        return NULL;
    }
    return obj->data.object.entries[index].value;
}

static bool olib_object_struct_grow(olib_object_t* obj, size_t min_capacity) {
    if (obj->data.object.capacity >= min_capacity) {
        return true;
    }
    size_t new_capacity = obj->data.object.capacity ? obj->data.object.capacity * 2 : 4;
    while (new_capacity < min_capacity) {
        new_capacity *= 2;
    }
    olib_struct_entry_t* new_entries = olib_realloc(obj->data.object.entries, new_capacity * sizeof(olib_struct_entry_t));
    if (!new_entries) {
        return false;
    }
    obj->data.object.entries = new_entries;
    obj->data.object.capacity = new_capacity;
    return true;
}

OLIB_API bool olib_object_struct_add(olib_object_t* obj, const char* key, olib_object_t* value) {
    if (!obj || obj->type != OLIB_OBJECT_TYPE_STRUCT || !key) {
        return false;
    }
    if (olib_object_struct_find(obj, key)) {
        return false;
    }
    if (!olib_object_struct_grow(obj, obj->data.object.size + 1)) {
        return false;
    }
    size_t key_len = strlen(key);
    char* key_copy = olib_malloc(key_len + 1);
    if (!key_copy) {
        return false;
    }
    memcpy(key_copy, key, key_len + 1);
    obj->data.object.entries[obj->data.object.size].key = key_copy;
    obj->data.object.entries[obj->data.object.size].value = value;
    obj->data.object.size++;
    return true;
}

OLIB_API bool olib_object_struct_set(olib_object_t* obj, const char* key, olib_object_t* value) {
    if (!obj || obj->type != OLIB_OBJECT_TYPE_STRUCT || !key) {
        return false;
    }
    olib_struct_entry_t* entry = olib_object_struct_find(obj, key);
    if (entry) {
        olib_object_free(entry->value);
        entry->value = value;
        return true;
    }
    return olib_object_struct_add(obj, key, value);
}

OLIB_API bool olib_object_struct_remove(olib_object_t* obj, const char* key) {
    if (!obj || obj->type != OLIB_OBJECT_TYPE_STRUCT || !key) {
        return false;
    }
    for (size_t i = 0; i < obj->data.object.size; i++) {
        if (strcmp(obj->data.object.entries[i].key, key) == 0) {
            olib_free(obj->data.object.entries[i].key);
            olib_object_free(obj->data.object.entries[i].value);
            for (size_t j = i; j < obj->data.object.size - 1; j++) {
                obj->data.object.entries[j] = obj->data.object.entries[j + 1];
            }
            obj->data.object.size--;
            return true;
        }
    }
    return false;
}

// #############################################################################
// Value getters
// #############################################################################

OLIB_API int64_t olib_object_get_int(olib_object_t* obj) {
    if (!obj) {
        return 0;
    }
    switch (obj->type) {
        case OLIB_OBJECT_TYPE_INT:
            return obj->data.int_val;
        case OLIB_OBJECT_TYPE_UINT:
            return (int64_t)obj->data.uint_val;
        case OLIB_OBJECT_TYPE_FLOAT:
            return (int64_t)obj->data.float_val;
        case OLIB_OBJECT_TYPE_BOOL:
            return obj->data.bool_val ? 1 : 0;
        case OLIB_OBJECT_TYPE_STRING:
            if (obj->data.string_val) {
                return strtoll(obj->data.string_val, NULL, 10);
            }
            return 0;
        default:
            return 0;
    }
}

OLIB_API uint64_t olib_object_get_uint(olib_object_t* obj) {
    if (!obj) {
        return 0;
    }
    switch (obj->type) {
        case OLIB_OBJECT_TYPE_UINT:
            return obj->data.uint_val;
        case OLIB_OBJECT_TYPE_INT:
            return (uint64_t)obj->data.int_val;
        case OLIB_OBJECT_TYPE_FLOAT:
            return (uint64_t)obj->data.float_val;
        case OLIB_OBJECT_TYPE_BOOL:
            return obj->data.bool_val ? 1 : 0;
        case OLIB_OBJECT_TYPE_STRING:
            if (obj->data.string_val) {
                return strtoull(obj->data.string_val, NULL, 10);
            }
            return 0;
        default:
            return 0;
    }
}

OLIB_API double olib_object_get_float(olib_object_t* obj) {
    if (!obj) {
        return 0.0;
    }
    switch (obj->type) {
        case OLIB_OBJECT_TYPE_FLOAT:
            return obj->data.float_val;
        case OLIB_OBJECT_TYPE_INT:
            return (double)obj->data.int_val;
        case OLIB_OBJECT_TYPE_UINT:
            return (double)obj->data.uint_val;
        case OLIB_OBJECT_TYPE_BOOL:
            return obj->data.bool_val ? 1.0 : 0.0;
        case OLIB_OBJECT_TYPE_STRING:
            if (obj->data.string_val) {
                return strtod(obj->data.string_val, NULL);
            }
            return 0.0;
        default:
            return 0.0;
    }
}

OLIB_API const char* olib_object_get_string(olib_object_t* obj) {
    if (!obj) {
        return NULL;
    }
    // Only return actual string values, no conversion for string getter
    if (obj->type == OLIB_OBJECT_TYPE_STRING) {
        return obj->data.string_val;
    }
    return NULL;
}

OLIB_API bool olib_object_get_bool(olib_object_t* obj) {
    if (!obj) {
        return false;
    }
    switch (obj->type) {
        case OLIB_OBJECT_TYPE_BOOL:
            return obj->data.bool_val;
        case OLIB_OBJECT_TYPE_INT:
            return obj->data.int_val != 0;
        case OLIB_OBJECT_TYPE_UINT:
            return obj->data.uint_val != 0;
        case OLIB_OBJECT_TYPE_FLOAT:
            return obj->data.float_val != 0.0;
        case OLIB_OBJECT_TYPE_STRING:
            if (obj->data.string_val) {
                return strcmp(obj->data.string_val, "true") == 0 ||
                       strcmp(obj->data.string_val, "1") == 0;
            }
            return false;
        default:
            return false;
    }
}

// #############################################################################
// Value setters
// #############################################################################

OLIB_API bool olib_object_set_int(olib_object_t* obj, int64_t value) {
    if (!obj || obj->type != OLIB_OBJECT_TYPE_INT) {
        return false;
    }
    obj->data.int_val = value;
    return true;
}

OLIB_API bool olib_object_set_uint(olib_object_t* obj, uint64_t value) {
    if (!obj || obj->type != OLIB_OBJECT_TYPE_UINT) {
        return false;
    }
    obj->data.uint_val = value;
    return true;
}

OLIB_API bool olib_object_set_float(olib_object_t* obj, double value) {
    if (!obj || obj->type != OLIB_OBJECT_TYPE_FLOAT) {
        return false;
    }
    obj->data.float_val = value;
    return true;
}

OLIB_API bool olib_object_set_string(olib_object_t* obj, const char* value) {
    if (!obj || obj->type != OLIB_OBJECT_TYPE_STRING) {
        return false;
    }
    if (obj->data.string_val) {
        olib_free(obj->data.string_val);
        obj->data.string_val = NULL;
    }
    if (value) {
        size_t len = strlen(value);
        obj->data.string_val = olib_malloc(len + 1);
        if (!obj->data.string_val) {
            return false;
        }
        memcpy(obj->data.string_val, value, len + 1);
    }
    return true;
}

OLIB_API bool olib_object_set_bool(olib_object_t* obj, bool value) {
    if (!obj || obj->type != OLIB_OBJECT_TYPE_BOOL) {
        return false;
    }
    obj->data.bool_val = value;
    return true;
}

// #############################################################################
// Matrix operations
// #############################################################################

OLIB_API olib_object_t* olib_object_matrix_new(size_t ndims, const size_t* dims) {
    if (ndims == 0 || !dims) {
        return NULL;
    }

    size_t total_size = 1;
    for (size_t i = 0; i < ndims; i++) {
        if (dims[i] == 0) {
            return NULL;
        }
        total_size *= dims[i];
    }

    olib_object_t* obj = olib_calloc(1, sizeof(olib_object_t));
    if (!obj) {
        return NULL;
    }
    obj->type = OLIB_OBJECT_TYPE_MATRIX;

    obj->data.matrix.dims = olib_malloc(ndims * sizeof(size_t));
    if (!obj->data.matrix.dims) {
        olib_free(obj);
        return NULL;
    }
    memcpy(obj->data.matrix.dims, dims, ndims * sizeof(size_t));

    obj->data.matrix.data = olib_calloc(total_size, sizeof(double));
    if (!obj->data.matrix.data) {
        olib_free(obj->data.matrix.dims);
        olib_free(obj);
        return NULL;
    }

    obj->data.matrix.ndims = ndims;
    obj->data.matrix.total_size = total_size;

    return obj;
}

OLIB_API size_t olib_object_matrix_ndims(olib_object_t* obj) {
    if (!obj || obj->type != OLIB_OBJECT_TYPE_MATRIX) {
        return 0;
    }
    return obj->data.matrix.ndims;
}

OLIB_API size_t olib_object_matrix_dim(olib_object_t* obj, size_t axis) {
    if (!obj || obj->type != OLIB_OBJECT_TYPE_MATRIX) {
        return 0;
    }
    if (axis >= obj->data.matrix.ndims) {
        return 0;
    }
    return obj->data.matrix.dims[axis];
}

OLIB_API const size_t* olib_object_matrix_dims(olib_object_t* obj) {
    if (!obj || obj->type != OLIB_OBJECT_TYPE_MATRIX) {
        return NULL;
    }
    return obj->data.matrix.dims;
}

OLIB_API size_t olib_object_matrix_total_size(olib_object_t* obj) {
    if (!obj || obj->type != OLIB_OBJECT_TYPE_MATRIX) {
        return 0;
    }
    return obj->data.matrix.total_size;
}

static size_t olib_object_matrix_calc_index(olib_object_t* obj, const size_t* indices) {
    size_t index = 0;
    size_t multiplier = 1;
    for (size_t i = obj->data.matrix.ndims; i > 0; i--) {
        size_t dim_idx = i - 1;
        if (indices[dim_idx] >= obj->data.matrix.dims[dim_idx]) {
            return (size_t)-1;
        }
        index += indices[dim_idx] * multiplier;
        multiplier *= obj->data.matrix.dims[dim_idx];
    }
    return index;
}

OLIB_API double olib_object_matrix_get(olib_object_t* obj, const size_t* indices) {
    if (!obj || obj->type != OLIB_OBJECT_TYPE_MATRIX || !indices) {
        return 0.0;
    }
    size_t index = olib_object_matrix_calc_index(obj, indices);
    if (index == (size_t)-1) {
        return 0.0;
    }
    return obj->data.matrix.data[index];
}

OLIB_API double* olib_object_matrix_data(olib_object_t* obj) {
    if (!obj || obj->type != OLIB_OBJECT_TYPE_MATRIX) {
        return NULL;
    }
    return obj->data.matrix.data;
}

OLIB_API bool olib_object_matrix_set(olib_object_t* obj, const size_t* indices, double value) {
    if (!obj || obj->type != OLIB_OBJECT_TYPE_MATRIX || !indices) {
        return false;
    }
    size_t index = olib_object_matrix_calc_index(obj, indices);
    if (index == (size_t)-1) {
        return false;
    }
    obj->data.matrix.data[index] = value;
    return true;
}

OLIB_API bool olib_object_matrix_fill(olib_object_t* obj, double value) {
    if (!obj || obj->type != OLIB_OBJECT_TYPE_MATRIX) {
        return false;
    }
    for (size_t i = 0; i < obj->data.matrix.total_size; i++) {
        obj->data.matrix.data[i] = value;
    }
    return true;
}

OLIB_API bool olib_object_matrix_set_data(olib_object_t* obj, const double* data, size_t count) {
    if (!obj || obj->type != OLIB_OBJECT_TYPE_MATRIX || !data) {
        return false;
    }
    size_t copy_count = count < obj->data.matrix.total_size ? count : obj->data.matrix.total_size;
    memcpy(obj->data.matrix.data, data, copy_count * sizeof(double));
    return true;
}
