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

TEST(ObjectCreation, CreateAllTypes) {
  olib_object_t* obj_struct = olib_object_new(OLIB_OBJECT_TYPE_STRUCT);
  olib_object_t* obj_list = olib_object_new(OLIB_OBJECT_TYPE_LIST);
  olib_object_t* obj_int = olib_object_new(OLIB_OBJECT_TYPE_INT);
  olib_object_t* obj_uint = olib_object_new(OLIB_OBJECT_TYPE_UINT);
  olib_object_t* obj_float = olib_object_new(OLIB_OBJECT_TYPE_FLOAT);
  olib_object_t* obj_string = olib_object_new(OLIB_OBJECT_TYPE_STRING);
  olib_object_t* obj_bool = olib_object_new(OLIB_OBJECT_TYPE_BOOL);

  ASSERT_NE(obj_struct, nullptr);
  ASSERT_NE(obj_list, nullptr);
  ASSERT_NE(obj_int, nullptr);
  ASSERT_NE(obj_uint, nullptr);
  ASSERT_NE(obj_float, nullptr);
  ASSERT_NE(obj_string, nullptr);
  ASSERT_NE(obj_bool, nullptr);

  olib_object_free(obj_struct);
  olib_object_free(obj_list);
  olib_object_free(obj_int);
  olib_object_free(obj_uint);
  olib_object_free(obj_float);
  olib_object_free(obj_string);
  olib_object_free(obj_bool);
}

TEST(ObjectCreation, TypeInspection) {
  olib_object_t* obj_struct = olib_object_new(OLIB_OBJECT_TYPE_STRUCT);
  olib_object_t* obj_list = olib_object_new(OLIB_OBJECT_TYPE_LIST);
  olib_object_t* obj_int = olib_object_new(OLIB_OBJECT_TYPE_INT);

  EXPECT_EQ(olib_object_get_type(obj_struct), OLIB_OBJECT_TYPE_STRUCT);
  EXPECT_EQ(olib_object_get_type(obj_list), OLIB_OBJECT_TYPE_LIST);
  EXPECT_EQ(olib_object_get_type(obj_int), OLIB_OBJECT_TYPE_INT);

  EXPECT_TRUE(olib_object_is_type(obj_struct, OLIB_OBJECT_TYPE_STRUCT));
  EXPECT_FALSE(olib_object_is_type(obj_struct, OLIB_OBJECT_TYPE_LIST));

  olib_object_free(obj_struct);
  olib_object_free(obj_list);
  olib_object_free(obj_int);
}

TEST(ObjectCreation, IsValueVsContainer) {
  olib_object_t* obj_struct = olib_object_new(OLIB_OBJECT_TYPE_STRUCT);
  olib_object_t* obj_list = olib_object_new(OLIB_OBJECT_TYPE_LIST);
  olib_object_t* obj_int = olib_object_new(OLIB_OBJECT_TYPE_INT);
  olib_object_t* obj_string = olib_object_new(OLIB_OBJECT_TYPE_STRING);

  EXPECT_TRUE(olib_object_is_container(obj_struct));
  EXPECT_TRUE(olib_object_is_container(obj_list));
  EXPECT_FALSE(olib_object_is_container(obj_int));
  EXPECT_FALSE(olib_object_is_container(obj_string));

  EXPECT_FALSE(olib_object_is_value(obj_struct));
  EXPECT_FALSE(olib_object_is_value(obj_list));
  EXPECT_TRUE(olib_object_is_value(obj_int));
  EXPECT_TRUE(olib_object_is_value(obj_string));

  olib_object_free(obj_struct);
  olib_object_free(obj_list);
  olib_object_free(obj_int);
  olib_object_free(obj_string);
}

TEST(ObjectCreation, TypeToString) {
  EXPECT_STREQ(olib_object_type_to_string(OLIB_OBJECT_TYPE_STRUCT), "struct");
  EXPECT_STREQ(olib_object_type_to_string(OLIB_OBJECT_TYPE_LIST), "list");
  EXPECT_STREQ(olib_object_type_to_string(OLIB_OBJECT_TYPE_INT), "int");
  EXPECT_STREQ(olib_object_type_to_string(OLIB_OBJECT_TYPE_UINT), "uint");
  EXPECT_STREQ(olib_object_type_to_string(OLIB_OBJECT_TYPE_FLOAT), "float");
  EXPECT_STREQ(olib_object_type_to_string(OLIB_OBJECT_TYPE_STRING), "string");
  EXPECT_STREQ(olib_object_type_to_string(OLIB_OBJECT_TYPE_BOOL), "bool");
}

TEST(ObjectCreation, FreeNullIsSafe) {
  olib_object_free(nullptr);  // Should not crash
}
