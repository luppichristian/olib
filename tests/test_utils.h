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

#include <gtest/gtest.h>
#include <olib.h>
#include <cmath>
#include <cstring>
#include <string>

// Helper function to create a test object with various types
// Thread safety: Concurrent calls are safe if the configured allocator supports
// them and is not changed concurrently. Each call returns a separate object.
inline olib_object_t* create_test_object() {
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

  olib_object_t* list_val = olib_object_new(OLIB_OBJECT_TYPE_LIST);
  for (int i = 0; i < 3; i++) {
    olib_object_t* elem = olib_object_new(OLIB_OBJECT_TYPE_INT);
    olib_object_set_int(elem, i * 100);
    olib_object_list_push(list_val, elem);
  }
  olib_object_struct_add(root, "list_val", list_val);

  olib_object_t* nested = olib_object_new(OLIB_OBJECT_TYPE_STRUCT);
  olib_object_t* nested_int = olib_object_new(OLIB_OBJECT_TYPE_INT);
  olib_object_set_int(nested_int, 999);
  olib_object_struct_add(nested, "nested_int", nested_int);
  olib_object_struct_add(root, "nested", nested);

  return root;
}

// Thread safety: The object tree must not be modified or freed during this
// call. Concurrent assertions are also subject to Google Test's guarantees on
// the platform in use.
inline void verify_test_object(olib_object_t* obj) {
  ASSERT_NE(obj, nullptr);
  ASSERT_EQ(olib_object_get_type(obj), OLIB_OBJECT_TYPE_STRUCT);
  ASSERT_EQ(olib_object_struct_size(obj), 7u);

  const char* keys[] = {"int_val", "uint_val", "float_val", "string_val", "bool_val", "list_val", "nested"};
  for (const char* key : keys) {
    EXPECT_NE(olib_object_struct_get(obj, key), nullptr) << key;
  }

  EXPECT_EQ(olib_object_get_type(olib_object_struct_get(obj, "int_val")), OLIB_OBJECT_TYPE_INT);
  const auto uint_type = olib_object_get_type(olib_object_struct_get(obj, "uint_val"));
  // Untagged text formats do not distinguish signed and unsigned integers.
  EXPECT_TRUE(uint_type == OLIB_OBJECT_TYPE_INT || uint_type == OLIB_OBJECT_TYPE_UINT);
  EXPECT_EQ(olib_object_get_type(olib_object_struct_get(obj, "float_val")), OLIB_OBJECT_TYPE_FLOAT);
  EXPECT_EQ(olib_object_get_type(olib_object_struct_get(obj, "string_val")), OLIB_OBJECT_TYPE_STRING);
  EXPECT_EQ(olib_object_get_type(olib_object_struct_get(obj, "bool_val")), OLIB_OBJECT_TYPE_BOOL);

  EXPECT_EQ(olib_object_get_int(olib_object_struct_get(obj, "int_val")), -42);
  EXPECT_EQ(olib_object_get_uint(olib_object_struct_get(obj, "uint_val")), 12345u);
  EXPECT_DOUBLE_EQ(olib_object_get_float(olib_object_struct_get(obj, "float_val")), 3.14159);
  EXPECT_STREQ(olib_object_get_string(olib_object_struct_get(obj, "string_val")), "Hello, World!");
  EXPECT_TRUE(olib_object_get_bool(olib_object_struct_get(obj, "bool_val")));

  olib_object_t* arr = olib_object_struct_get(obj, "list_val");
  ASSERT_NE(arr, nullptr);
  ASSERT_EQ(olib_object_get_type(arr), OLIB_OBJECT_TYPE_LIST);
  ASSERT_EQ(olib_object_list_size(arr), 3u);
  for (int i = 0; i < 3; i++) {
    ASSERT_NE(olib_object_list_get(arr, i), nullptr);
    EXPECT_EQ(olib_object_get_type(olib_object_list_get(arr, i)), OLIB_OBJECT_TYPE_INT);
    EXPECT_EQ(olib_object_get_int(olib_object_list_get(arr, i)), i * 100);
  }

  olib_object_t* nested = olib_object_struct_get(obj, "nested");
  ASSERT_NE(nested, nullptr);
  ASSERT_EQ(olib_object_get_type(nested), OLIB_OBJECT_TYPE_STRUCT);
  ASSERT_EQ(olib_object_struct_size(nested), 1u);
  ASSERT_NE(olib_object_struct_get(nested, "nested_int"), nullptr);
  EXPECT_EQ(olib_object_get_type(olib_object_struct_get(nested, "nested_int")), OLIB_OBJECT_TYPE_INT);
  EXPECT_EQ(olib_object_get_int(olib_object_struct_get(nested, "nested_int")), 999);
}

// Compare every node and key without relying on coercing getters for its type.
// JSON, YAML, TOML and TXT encode the fixture's small unsigned integer without
// a type tag, so their readers represent it as a signed integer. XML and the
// binary formats preserve the unsigned type.
inline void expect_round_trip_tree(olib_object_t* expected, olib_object_t* actual, olib_format_t format) {
  ASSERT_NE(expected, nullptr);
  ASSERT_NE(actual, nullptr);
  const auto expected_type = olib_object_get_type(expected);
  const bool untagged_integer = format == OLIB_FORMAT_JSON_TEXT || format == OLIB_FORMAT_YAML ||
                                format == OLIB_FORMAT_TOML || format == OLIB_FORMAT_TXT;
  const auto actual_type = expected_type == OLIB_OBJECT_TYPE_UINT && untagged_integer
                               ? OLIB_OBJECT_TYPE_INT : expected_type;
  ASSERT_EQ(olib_object_get_type(actual), actual_type);

  switch (expected_type) {
    case OLIB_OBJECT_TYPE_INT:
      EXPECT_EQ(olib_object_get_int(actual), olib_object_get_int(expected));
      break;
    case OLIB_OBJECT_TYPE_UINT:
      EXPECT_EQ(olib_object_get_uint(actual), olib_object_get_uint(expected));
      break;
    case OLIB_OBJECT_TYPE_FLOAT:
      EXPECT_DOUBLE_EQ(olib_object_get_float(actual), olib_object_get_float(expected));
      break;
    case OLIB_OBJECT_TYPE_STRING:
      EXPECT_STREQ(olib_object_get_string(actual), olib_object_get_string(expected));
      break;
    case OLIB_OBJECT_TYPE_BOOL:
      EXPECT_EQ(olib_object_get_bool(actual), olib_object_get_bool(expected));
      break;
    case OLIB_OBJECT_TYPE_LIST: {
      const size_t size = olib_object_list_size(expected);
      ASSERT_EQ(olib_object_list_size(actual), size);
      for (size_t i = 0; i < size; ++i) {
        SCOPED_TRACE(::testing::Message() << "list index " << i);
        expect_round_trip_tree(olib_object_list_get(expected, i), olib_object_list_get(actual, i), format);
      }
      break;
    }
    case OLIB_OBJECT_TYPE_STRUCT: {
      const size_t size = olib_object_struct_size(expected);
      ASSERT_EQ(olib_object_struct_size(actual), size);
      for (size_t i = 0; i < size; ++i) {
        const char* key = olib_object_struct_key_at(expected, i);
        ASSERT_NE(key, nullptr);
        SCOPED_TRACE(::testing::Message() << "struct key " << key);
        expect_round_trip_tree(olib_object_struct_value_at(expected, i), olib_object_struct_get(actual, key), format);
      }
      break;
    }
    default:
      FAIL() << "Unexpected object type " << expected_type;
  }
}
