#include <gtest/gtest.h>
#include <olib.h>
#include <cstdio>
#include <cstring>
#include <string>

// =============================================================================
// Null Input Handling Tests
// =============================================================================

TEST(EdgeCases, NullInputsToFunctions) {
  // These should handle null gracefully (not crash)
  EXPECT_EQ(olib_object_get_type(nullptr), OLIB_OBJECT_TYPE_MAX);  // Invalid type for null input
  EXPECT_FALSE(olib_object_is_type(nullptr, OLIB_OBJECT_TYPE_INT));
  EXPECT_EQ(olib_object_list_size(nullptr), 0u);
  EXPECT_EQ(olib_object_struct_size(nullptr), 0u);
  EXPECT_FALSE(olib_object_struct_has(nullptr, "key"));
  EXPECT_EQ(olib_object_struct_get(nullptr, "key"), nullptr);
}

// =============================================================================
// Large Data Tests
// =============================================================================

TEST(EdgeCases, LargeList) {
  olib_object_t* arr = olib_object_new(OLIB_OBJECT_TYPE_LIST);

  const int COUNT = 1000;
  for (int i = 0; i < COUNT; i++) {
    olib_object_t* val = olib_object_new(OLIB_OBJECT_TYPE_INT);
    olib_object_set_int(val, i);
    EXPECT_TRUE(olib_object_list_push(arr, val));
  }

  EXPECT_EQ(olib_object_list_size(arr), (size_t)COUNT);

  for (int i = 0; i < COUNT; i++) {
    EXPECT_EQ(olib_object_get_int(olib_object_list_get(arr, i)), i);
  }

  olib_object_free(arr);
}

TEST(EdgeCases, LargeStruct) {
  olib_object_t* obj = olib_object_new(OLIB_OBJECT_TYPE_STRUCT);

  const int COUNT = 500;
  for (int i = 0; i < COUNT; i++) {
    char key[32];
    snprintf(key, sizeof(key), "key_%d", i);
    olib_object_t* val = olib_object_new(OLIB_OBJECT_TYPE_INT);
    olib_object_set_int(val, i);
    EXPECT_TRUE(olib_object_struct_add(obj, key, val));
  }

  EXPECT_EQ(olib_object_struct_size(obj), (size_t)COUNT);

  for (int i = 0; i < COUNT; i++) {
    char key[32];
    snprintf(key, sizeof(key), "key_%d", i);
    EXPECT_TRUE(olib_object_struct_has(obj, key));
    EXPECT_EQ(olib_object_get_int(olib_object_struct_get(obj, key)), i);
  }

  olib_object_free(obj);
}

// =============================================================================
// Deeply Nested Structure Tests
// =============================================================================

TEST(EdgeCases, DeeplyNestedStructure) {
  olib_object_t* root = olib_object_new(OLIB_OBJECT_TYPE_STRUCT);
  olib_object_t* current = root;

  const int DEPTH = 50;
  for (int i = 0; i < DEPTH; i++) {
    olib_object_t* child = olib_object_new(OLIB_OBJECT_TYPE_STRUCT);
    olib_object_struct_add(current, "child", child);
    current = child;
  }

  olib_object_t* leaf = olib_object_new(OLIB_OBJECT_TYPE_INT);
  olib_object_set_int(leaf, 42);
  olib_object_struct_add(current, "value", leaf);

  // Traverse back to verify
  current = root;
  for (int i = 0; i < DEPTH; i++) {
    current = olib_object_struct_get(current, "child");
    ASSERT_NE(current, nullptr);
  }
  EXPECT_EQ(olib_object_get_int(olib_object_struct_get(current, "value")), 42);

  olib_object_free(root);
}

TEST(EdgeCases, DeeplyNestedList) {
  olib_object_t* root = olib_object_new(OLIB_OBJECT_TYPE_LIST);
  olib_object_t* current = root;

  const int DEPTH = 50;
  for (int i = 0; i < DEPTH; i++) {
    olib_object_t* child = olib_object_new(OLIB_OBJECT_TYPE_LIST);
    olib_object_list_push(current, child);
    current = child;
  }

  olib_object_t* leaf = olib_object_new(OLIB_OBJECT_TYPE_INT);
  olib_object_set_int(leaf, 999);
  olib_object_list_push(current, leaf);

  // Traverse back to verify
  current = root;
  for (int i = 0; i < DEPTH; i++) {
    current = olib_object_list_get(current, 0);
    ASSERT_NE(current, nullptr);
  }
  EXPECT_EQ(olib_object_get_int(olib_object_list_get(current, 0)), 999);

  olib_object_free(root);
}

// =============================================================================
// String Edge Cases
// =============================================================================

TEST(EdgeCases, EmptyString) {
  olib_object_t* obj = olib_object_new(OLIB_OBJECT_TYPE_STRING);
  EXPECT_TRUE(olib_object_set_string(obj, ""));
  EXPECT_STREQ(olib_object_get_string(obj), "");

  // Serialize and deserialize empty string
  olib_serializer_t* ser = olib_serializer_new_json_text();
  char* json = nullptr;
  EXPECT_TRUE(olib_serializer_write_string(ser, obj, &json));

  olib_object_t* parsed = olib_serializer_read_string(ser, json);
  ASSERT_NE(parsed, nullptr);
  EXPECT_STREQ(olib_object_get_string(parsed), "");

  olib_free(json);
  olib_object_free(obj);
  olib_object_free(parsed);
  olib_serializer_free(ser);
}

TEST(EdgeCases, VeryLongString) {
  olib_object_t* obj = olib_object_new(OLIB_OBJECT_TYPE_STRING);

  std::string long_str(10000, 'x');
  EXPECT_TRUE(olib_object_set_string(obj, long_str.c_str()));
  EXPECT_EQ(strlen(olib_object_get_string(obj)), 10000u);

  olib_object_free(obj);
}

TEST(EdgeCases, StringWithNullCharacter) {
  olib_object_t* obj = olib_object_new(OLIB_OBJECT_TYPE_STRING);

  // String with embedded special characters (but not null, as C strings terminate at null)
  EXPECT_TRUE(olib_object_set_string(obj,
                                     "before\x01\x02\x03"
                                     "after"));
  EXPECT_STREQ(olib_object_get_string(obj),
               "before\x01\x02\x03"
               "after");

  olib_object_free(obj);
}

TEST(EdgeCases, StringWithUnicode) {
  olib_object_t* obj = olib_object_new(OLIB_OBJECT_TYPE_STRING);

  // UTF-8 encoded strings (ä¸–ç•Œ = \xE4\xB8\x96\xE7\x95\x8C, ðŸŒ = \xF0\x9F\x8C\x8D)
  const char* unicode_str = "Hello \xE4\xB8\x96\xE7\x95\x8C \xF0\x9F\x8C\x8D";
  EXPECT_TRUE(olib_object_set_string(obj, unicode_str));
  EXPECT_STREQ(olib_object_get_string(obj), unicode_str);

  olib_object_free(obj);
}

// =============================================================================
// Numeric Edge Cases
// =============================================================================

TEST(EdgeCases, IntBoundaryValues) {
  olib_object_t* obj = olib_object_new(OLIB_OBJECT_TYPE_INT);

  EXPECT_TRUE(olib_object_set_int(obj, 0));
  EXPECT_EQ(olib_object_get_int(obj), 0);

  EXPECT_TRUE(olib_object_set_int(obj, 1));
  EXPECT_EQ(olib_object_get_int(obj), 1);

  EXPECT_TRUE(olib_object_set_int(obj, -1));
  EXPECT_EQ(olib_object_get_int(obj), -1);

  EXPECT_TRUE(olib_object_set_int(obj, INT64_MAX));
  EXPECT_EQ(olib_object_get_int(obj), INT64_MAX);

  EXPECT_TRUE(olib_object_set_int(obj, INT64_MIN));
  EXPECT_EQ(olib_object_get_int(obj), INT64_MIN);

  olib_object_free(obj);
}

TEST(EdgeCases, UintBoundaryValues) {
  olib_object_t* obj = olib_object_new(OLIB_OBJECT_TYPE_UINT);

  EXPECT_TRUE(olib_object_set_uint(obj, 0));
  EXPECT_EQ(olib_object_get_uint(obj), 0u);

  EXPECT_TRUE(olib_object_set_uint(obj, 1));
  EXPECT_EQ(olib_object_get_uint(obj), 1u);

  EXPECT_TRUE(olib_object_set_uint(obj, UINT64_MAX));
  EXPECT_EQ(olib_object_get_uint(obj), UINT64_MAX);

  olib_object_free(obj);
}

TEST(EdgeCases, FloatSpecialValues) {
  olib_object_t* obj = olib_object_new(OLIB_OBJECT_TYPE_FLOAT);

  // Very small value
  EXPECT_TRUE(olib_object_set_float(obj, 1e-308));
  EXPECT_DOUBLE_EQ(olib_object_get_float(obj), 1e-308);

  // Very large value
  EXPECT_TRUE(olib_object_set_float(obj, 1e308));
  EXPECT_DOUBLE_EQ(olib_object_get_float(obj), 1e308);

  // Negative zero
  EXPECT_TRUE(olib_object_set_float(obj, -0.0));
  // Note: -0.0 and 0.0 are equal in comparisons

  olib_object_free(obj);
}

// =============================================================================
// Empty Container Tests
// =============================================================================

TEST(EdgeCases, EmptyListOperations) {
  olib_object_t* arr = olib_object_new(OLIB_OBJECT_TYPE_LIST);

  EXPECT_EQ(olib_object_list_size(arr), 0u);
  EXPECT_EQ(olib_object_list_get(arr, 0), nullptr);
  EXPECT_FALSE(olib_object_list_pop(arr));
  EXPECT_FALSE(olib_object_list_remove(arr, 0));

  olib_object_free(arr);
}

TEST(EdgeCases, EmptyStructOperations) {
  olib_object_t* obj = olib_object_new(OLIB_OBJECT_TYPE_STRUCT);

  EXPECT_EQ(olib_object_struct_size(obj), 0u);
  EXPECT_EQ(olib_object_struct_get(obj, "any"), nullptr);
  EXPECT_FALSE(olib_object_struct_has(obj, "any"));
  EXPECT_FALSE(olib_object_struct_remove(obj, "any"));
  EXPECT_EQ(olib_object_struct_key_at(obj, 0), nullptr);
  EXPECT_EQ(olib_object_struct_value_at(obj, 0), nullptr);

  olib_object_free(obj);
}

// =============================================================================
// Serializer Edge Cases
// =============================================================================

TEST(EdgeCases, SerializeEmptyContainers) {
  olib_serializer_t* ser = olib_serializer_new_json_text();

  // Empty list
  olib_object_t* empty_lst = olib_object_new(OLIB_OBJECT_TYPE_LIST);
  char* json = nullptr;
  EXPECT_TRUE(olib_serializer_write_string(ser, empty_lst, &json));
  ASSERT_NE(json, nullptr);

  olib_object_t* parsed_lst = olib_serializer_read_string(ser, json);
  ASSERT_NE(parsed_lst, nullptr);
  EXPECT_EQ(olib_object_list_size(parsed_lst), 0u);

  olib_free(json);
  olib_object_free(empty_lst);
  olib_object_free(parsed_lst);

  // Empty struct
  olib_object_t* empty_struct = olib_object_new(OLIB_OBJECT_TYPE_STRUCT);
  json = nullptr;
  EXPECT_TRUE(olib_serializer_write_string(ser, empty_struct, &json));
  ASSERT_NE(json, nullptr);

  olib_object_t* parsed_struct = olib_serializer_read_string(ser, json);
  ASSERT_NE(parsed_struct, nullptr);
  EXPECT_EQ(olib_object_struct_size(parsed_struct), 0u);

  olib_free(json);
  olib_object_free(empty_struct);
  olib_object_free(parsed_struct);

  olib_serializer_free(ser);
}

TEST(EdgeCases, SerializePrimitiveTypes) {
  olib_serializer_t* ser = olib_serializer_new_json_text();

  // Boolean true
  olib_object_t* bool_true = olib_object_new(OLIB_OBJECT_TYPE_BOOL);
  olib_object_set_bool(bool_true, true);
  char* json = nullptr;
  EXPECT_TRUE(olib_serializer_write_string(ser, bool_true, &json));
  olib_object_t* parsed = olib_serializer_read_string(ser, json);
  EXPECT_TRUE(olib_object_get_bool(parsed));
  olib_free(json);
  olib_object_free(bool_true);
  olib_object_free(parsed);

  // Boolean false
  olib_object_t* bool_false = olib_object_new(OLIB_OBJECT_TYPE_BOOL);
  olib_object_set_bool(bool_false, false);
  json = nullptr;
  EXPECT_TRUE(olib_serializer_write_string(ser, bool_false, &json));
  parsed = olib_serializer_read_string(ser, json);
  EXPECT_FALSE(olib_object_get_bool(parsed));
  olib_free(json);
  olib_object_free(bool_false);
  olib_object_free(parsed);

  olib_serializer_free(ser);
}
