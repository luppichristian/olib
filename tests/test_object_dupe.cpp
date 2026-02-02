#include <gtest/gtest.h>
#include <olib.h>

TEST(ObjectDupe, DupeInt) {
  olib_object_t* original = olib_object_new(OLIB_OBJECT_TYPE_INT);
  olib_object_set_int(original, 12345);

  olib_object_t* copy = olib_object_dupe(original);
  ASSERT_NE(copy, nullptr);
  EXPECT_NE(copy, original);
  EXPECT_EQ(olib_object_get_type(copy), OLIB_OBJECT_TYPE_INT);
  EXPECT_EQ(olib_object_get_int(copy), 12345);

  // Modifying copy shouldn't affect original
  olib_object_set_int(copy, 99999);
  EXPECT_EQ(olib_object_get_int(original), 12345);
  EXPECT_EQ(olib_object_get_int(copy), 99999);

  olib_object_free(original);
  olib_object_free(copy);
}

TEST(ObjectDupe, DupeString) {
  olib_object_t* original = olib_object_new(OLIB_OBJECT_TYPE_STRING);
  olib_object_set_string(original, "Hello World");

  olib_object_t* copy = olib_object_dupe(original);
  ASSERT_NE(copy, nullptr);
  EXPECT_STREQ(olib_object_get_string(copy), "Hello World");

  olib_object_set_string(copy, "Changed");
  EXPECT_STREQ(olib_object_get_string(original), "Hello World");

  olib_object_free(original);
  olib_object_free(copy);
}

TEST(ObjectDupe, DupeList) {
  olib_object_t* original = olib_object_new(OLIB_OBJECT_TYPE_LIST);

  for (int i = 0; i < 3; i++) {
    olib_object_t* val = olib_object_new(OLIB_OBJECT_TYPE_INT);
    olib_object_set_int(val, i * 10);
    olib_object_list_push(original, val);
  }

  olib_object_t* copy = olib_object_dupe(original);
  ASSERT_NE(copy, nullptr);
  EXPECT_EQ(olib_object_list_size(copy), 3u);

  for (int i = 0; i < 3; i++) {
    EXPECT_EQ(olib_object_get_int(olib_object_list_get(copy, i)), i * 10);
  }

  // Modifying copy shouldn't affect original (deep copy)
  olib_object_set_int(olib_object_list_get(copy, 0), 999);
  EXPECT_EQ(olib_object_get_int(olib_object_list_get(original, 0)), 0);

  olib_object_free(original);
  olib_object_free(copy);
}

TEST(ObjectDupe, DupeStruct) {
  olib_object_t* original = olib_object_new(OLIB_OBJECT_TYPE_STRUCT);

  olib_object_t* val1 = olib_object_new(OLIB_OBJECT_TYPE_INT);
  olib_object_t* val2 = olib_object_new(OLIB_OBJECT_TYPE_STRING);
  olib_object_set_int(val1, 42);
  olib_object_set_string(val2, "test");

  olib_object_struct_add(original, "number", val1);
  olib_object_struct_add(original, "text", val2);

  olib_object_t* copy = olib_object_dupe(original);
  ASSERT_NE(copy, nullptr);
  EXPECT_EQ(olib_object_struct_size(copy), 2u);
  EXPECT_EQ(olib_object_get_int(olib_object_struct_get(copy, "number")), 42);
  EXPECT_STREQ(olib_object_get_string(olib_object_struct_get(copy, "text")), "test");

  olib_object_free(original);
  olib_object_free(copy);
}

TEST(ObjectDupe, DupeNested) {
  // Create nested structure: struct containing list containing struct
  olib_object_t* root = olib_object_new(OLIB_OBJECT_TYPE_STRUCT);
  olib_object_t* arr = olib_object_new(OLIB_OBJECT_TYPE_LIST);
  olib_object_t* inner = olib_object_new(OLIB_OBJECT_TYPE_STRUCT);

  olib_object_t* inner_val = olib_object_new(OLIB_OBJECT_TYPE_INT);
  olib_object_set_int(inner_val, 999);
  olib_object_struct_add(inner, "deep", inner_val);

  olib_object_list_push(arr, inner);
  olib_object_struct_add(root, "list", arr);

  olib_object_t* copy = olib_object_dupe(root);
  ASSERT_NE(copy, nullptr);

  olib_object_t* copy_arr = olib_object_struct_get(copy, "list");
  ASSERT_NE(copy_arr, nullptr);
  olib_object_t* copy_inner = olib_object_list_get(copy_arr, 0);
  ASSERT_NE(copy_inner, nullptr);
  EXPECT_EQ(olib_object_get_int(olib_object_struct_get(copy_inner, "deep")), 999);

  olib_object_free(root);
  olib_object_free(copy);
}

TEST(ObjectDupe, DupeNullReturnsNull) {
  EXPECT_EQ(olib_object_dupe(nullptr), nullptr);
}
