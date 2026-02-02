#pragma once

#include <gtest/gtest.h>
#include <olib.h>
#include <cstring>
#include <cmath>
#include <string>

// Helper function to create a test object with various types
inline olib_object_t* create_test_object()
{
    olib_object_t* root = olib_object_new(OLIB_OBJECT_TYPE_STRUCT);

    olib_object_t* int_val = olib_object_new(OLIB_OBJECT_TYPE_INT);
    olib_object_set_int(int_val, -42);
    olib_object_struct_add(root, "int_val", int_val);

    olib_object_t* uint_val = olib_object_new(OLIB_OBJECT_TYPE_UINT);
    olib_object_set_uint(uint_val, 12345);
    olib_object_struct_add(root, "uint_val", uint_val);

    olib_object_t* float_val = olib_object_new(OLIB_OBJECT_TYPE_FLOAT);
    olib_object_set_float(float_val, 3.14159);
    olib_object_struct_add(root, "float_val", float_val);

    olib_object_t* string_val = olib_object_new(OLIB_OBJECT_TYPE_STRING);
    olib_object_set_string(string_val, "Hello, World!");
    olib_object_struct_add(root, "string_val", string_val);

    olib_object_t* bool_val = olib_object_new(OLIB_OBJECT_TYPE_BOOL);
    olib_object_set_bool(bool_val, true);
    olib_object_struct_add(root, "bool_val", bool_val);

    olib_object_t* array_val = olib_object_new(OLIB_OBJECT_TYPE_ARRAY);
    for (int i = 0; i < 3; i++) {
        olib_object_t* elem = olib_object_new(OLIB_OBJECT_TYPE_INT);
        olib_object_set_int(elem, i * 100);
        olib_object_array_push(array_val, elem);
    }
    olib_object_struct_add(root, "array_val", array_val);

    olib_object_t* nested = olib_object_new(OLIB_OBJECT_TYPE_STRUCT);
    olib_object_t* nested_int = olib_object_new(OLIB_OBJECT_TYPE_INT);
    olib_object_set_int(nested_int, 999);
    olib_object_struct_add(nested, "nested_int", nested_int);
    olib_object_struct_add(root, "nested", nested);

    return root;
}

inline void verify_test_object(olib_object_t* obj)
{
    ASSERT_NE(obj, nullptr);
    EXPECT_EQ(olib_object_get_type(obj), OLIB_OBJECT_TYPE_STRUCT);

    EXPECT_EQ(olib_object_get_int(olib_object_struct_get(obj, "int_val")), -42);
    EXPECT_EQ(olib_object_get_uint(olib_object_struct_get(obj, "uint_val")), 12345u);
    EXPECT_NEAR(olib_object_get_float(olib_object_struct_get(obj, "float_val")), 3.14159, 0.00001);
    EXPECT_STREQ(olib_object_get_string(olib_object_struct_get(obj, "string_val")), "Hello, World!");
    EXPECT_TRUE(olib_object_get_bool(olib_object_struct_get(obj, "bool_val")));

    olib_object_t* arr = olib_object_struct_get(obj, "array_val");
    ASSERT_NE(arr, nullptr);
    EXPECT_EQ(olib_object_array_size(arr), 3u);
    for (int i = 0; i < 3; i++) {
        EXPECT_EQ(olib_object_get_int(olib_object_array_get(arr, i)), i * 100);
    }

    olib_object_t* nested = olib_object_struct_get(obj, "nested");
    ASSERT_NE(nested, nullptr);
    EXPECT_EQ(olib_object_get_int(olib_object_struct_get(nested, "nested_int")), 999);
}
