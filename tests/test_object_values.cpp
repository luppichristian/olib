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
#include <gtest/gtest.h>
#include <olib.h>
#include <cstring>

TEST(ObjectValues, IntGetSet) {
  olib_object_t* obj = olib_object_new(OLIB_OBJECT_TYPE_INT);
  ASSERT_NE(obj, nullptr);

  // Default value should be 0
  EXPECT_EQ(olib_object_get_int(obj), 0);

  // Set and get positive value
  EXPECT_TRUE(olib_object_set_int(obj, 42));
  EXPECT_EQ(olib_object_get_int(obj), 42);

  // Set and get negative value
  EXPECT_TRUE(olib_object_set_int(obj, -12345));
  EXPECT_EQ(olib_object_get_int(obj), -12345);

  // Set and get max/min values
  EXPECT_TRUE(olib_object_set_int(obj, INT64_MAX));
  EXPECT_EQ(olib_object_get_int(obj), INT64_MAX);

  EXPECT_TRUE(olib_object_set_int(obj, INT64_MIN));
  EXPECT_EQ(olib_object_get_int(obj), INT64_MIN);

  olib_object_free(obj);
}

TEST(ObjectValues, UintGetSet) {
  olib_object_t* obj = olib_object_new(OLIB_OBJECT_TYPE_UINT);
  ASSERT_NE(obj, nullptr);

  EXPECT_EQ(olib_object_get_uint(obj), 0u);

  EXPECT_TRUE(olib_object_set_uint(obj, 42));
  EXPECT_EQ(olib_object_get_uint(obj), 42u);

  EXPECT_TRUE(olib_object_set_uint(obj, UINT64_MAX));
  EXPECT_EQ(olib_object_get_uint(obj), UINT64_MAX);

  olib_object_free(obj);
}

TEST(ObjectValues, FloatGetSet) {
  olib_object_t* obj = olib_object_new(OLIB_OBJECT_TYPE_FLOAT);
  ASSERT_NE(obj, nullptr);

  EXPECT_DOUBLE_EQ(olib_object_get_float(obj), 0.0);

  EXPECT_TRUE(olib_object_set_float(obj, 3.14159));
  EXPECT_DOUBLE_EQ(olib_object_get_float(obj), 3.14159);

  EXPECT_TRUE(olib_object_set_float(obj, -2.71828));
  EXPECT_DOUBLE_EQ(olib_object_get_float(obj), -2.71828);

  // Test very small and large values
  EXPECT_TRUE(olib_object_set_float(obj, 1e-300));
  EXPECT_DOUBLE_EQ(olib_object_get_float(obj), 1e-300);

  EXPECT_TRUE(olib_object_set_float(obj, 1e300));
  EXPECT_DOUBLE_EQ(olib_object_get_float(obj), 1e300);

  olib_object_free(obj);
}

TEST(ObjectValues, StringGetSet) {
  olib_object_t* obj = olib_object_new(OLIB_OBJECT_TYPE_STRING);
  ASSERT_NE(obj, nullptr);

  // Default value should be empty string or null
  const char* default_str = olib_object_get_string(obj);
  EXPECT_TRUE(default_str == nullptr || strlen(default_str) == 0);

  // Set and get string
  EXPECT_TRUE(olib_object_set_string(obj, "Hello, World!"));
  EXPECT_STREQ(olib_object_get_string(obj), "Hello, World!");

  // Set and get empty string
  EXPECT_TRUE(olib_object_set_string(obj, ""));
  EXPECT_STREQ(olib_object_get_string(obj), "");

  // Set and get string with special characters
  EXPECT_TRUE(olib_object_set_string(obj, "Line1\nLine2\tTabbed\"Quoted\""));
  EXPECT_STREQ(olib_object_get_string(obj), "Line1\nLine2\tTabbed\"Quoted\"");

  // Unicode string
  EXPECT_TRUE(olib_object_set_string(obj, "Unicode: \xC3\xA9\xC3\xA8\xC3\xA0"));
  EXPECT_STREQ(olib_object_get_string(obj), "Unicode: \xC3\xA9\xC3\xA8\xC3\xA0");

  olib_object_free(obj);
}

TEST(ObjectValues, BoolGetSet) {
  olib_object_t* obj = olib_object_new(OLIB_OBJECT_TYPE_BOOL);
  ASSERT_NE(obj, nullptr);

  EXPECT_FALSE(olib_object_get_bool(obj));

  EXPECT_TRUE(olib_object_set_bool(obj, true));
  EXPECT_TRUE(olib_object_get_bool(obj));

  EXPECT_TRUE(olib_object_set_bool(obj, false));
  EXPECT_FALSE(olib_object_get_bool(obj));

  olib_object_free(obj);
}

TEST(ObjectValues, SetWrongTypeFails) {
  olib_object_t* obj_int = olib_object_new(OLIB_OBJECT_TYPE_INT);
  olib_object_t* obj_string = olib_object_new(OLIB_OBJECT_TYPE_STRING);

  // Setting string on int should fail
  EXPECT_FALSE(olib_object_set_string(obj_int, "test"));

  // Setting int on string should fail
  EXPECT_FALSE(olib_object_set_int(obj_string, 42));

  olib_object_free(obj_int);
  olib_object_free(obj_string);
}
