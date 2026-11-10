#include "test_utils.h"

TEST(YamlKeys, ArbitraryNamesRoundTripAsStrings) {
  const char* keys[] = {"simple", "a space", "x: y", "\"quoted\"", "", "true", "123",
                        "line\nbreak", "tab\tkey", "hash#tag", "trailing "};
  auto* root = olib_object_new(OLIB_OBJECT_TYPE_STRUCT);
  ASSERT_NE(root, nullptr);
  for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); ++i) {
    auto* item = olib_object_new(OLIB_OBJECT_TYPE_INT);
    ASSERT_NE(item, nullptr);
    ASSERT_TRUE(olib_object_set_int(item, static_cast<int64_t>(i)));
    ASSERT_TRUE(olib_object_struct_add(root, keys[i], item));
  }
  char* encoded = nullptr;
  ASSERT_TRUE(olib_format_write_string(OLIB_FORMAT_YAML, root, &encoded));
  ASSERT_NE(encoded, nullptr);
  auto* parsed = olib_format_read_string(OLIB_FORMAT_YAML, encoded);
  ASSERT_NE(parsed, nullptr) << encoded;
  ASSERT_EQ(olib_object_get_type(parsed), OLIB_OBJECT_TYPE_STRUCT) << encoded;
  ASSERT_EQ(olib_object_struct_size(parsed), sizeof(keys) / sizeof(keys[0])) << encoded;
  for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); ++i) {
    SCOPED_TRACE(keys[i]);
    auto* item = olib_object_struct_get(parsed, keys[i]);
    ASSERT_NE(item, nullptr) << encoded;
    EXPECT_EQ(olib_object_get_type(item), OLIB_OBJECT_TYPE_INT);
    EXPECT_EQ(olib_object_get_int(item), static_cast<int64_t>(i));
  }
  olib_object_free(parsed);
  olib_free(encoded);
  olib_object_free(root);
}

TEST(YamlKeys, ReadsPlainKeysContainingSpaces) {
  auto* parsed = olib_format_read_string(OLIB_FORMAT_YAML, "a space: 7\ntrue: 8\n");
  ASSERT_NE(parsed, nullptr);
  ASSERT_EQ(olib_object_get_type(parsed), OLIB_OBJECT_TYPE_STRUCT);
  ASSERT_EQ(olib_object_struct_size(parsed), 2u);
  EXPECT_EQ(olib_object_get_int(olib_object_struct_get(parsed, "a space")), 7);
  EXPECT_EQ(olib_object_get_int(olib_object_struct_get(parsed, "true")), 8);
  olib_object_free(parsed);
}

TEST(YamlKeys, PreservesStringValuesWithWhitespaceAndBooleanPrefixes) {
  const char* values[] = {" leading", "trailing ", "trueish", "false alarm", "none", "onward"};
  for (const char* expected : values) {
    SCOPED_TRACE(expected);
    auto* value = olib_object_new(OLIB_OBJECT_TYPE_STRING);
    ASSERT_NE(value, nullptr);
    ASSERT_TRUE(olib_object_set_string(value, expected));
    char* encoded = nullptr;
    ASSERT_TRUE(olib_format_write_string(OLIB_FORMAT_YAML, value, &encoded));
    ASSERT_NE(encoded, nullptr);
    auto* parsed = olib_format_read_string(OLIB_FORMAT_YAML, encoded);
    ASSERT_NE(parsed, nullptr) << encoded;
    EXPECT_EQ(olib_object_get_type(parsed), OLIB_OBJECT_TYPE_STRING) << encoded;
    EXPECT_STREQ(olib_object_get_string(parsed), expected);
    olib_object_free(parsed);
    olib_free(encoded);
    olib_object_free(value);
  }
}

TEST(YamlKeys, RejectsUnrepresentableControlCharactersInKeysAndValues) {
  for (const char* invalid : {"a\x01" "b", "\x7f"}) {
    auto* root = olib_object_new(OLIB_OBJECT_TYPE_STRUCT);
    auto* value = olib_object_new(OLIB_OBJECT_TYPE_STRING);
    ASSERT_NE(root, nullptr);
    ASSERT_NE(value, nullptr);
    ASSERT_TRUE(olib_object_set_string(value, "ok"));
    ASSERT_TRUE(olib_object_struct_add(root, invalid, value));
    char* encoded = nullptr;
    EXPECT_FALSE(olib_format_write_string(OLIB_FORMAT_YAML, root, &encoded));
    EXPECT_EQ(encoded, nullptr);
    olib_free(encoded);
    olib_object_free(root);

    value = olib_object_new(OLIB_OBJECT_TYPE_STRING);
    ASSERT_NE(value, nullptr);
    ASSERT_TRUE(olib_object_set_string(value, invalid));
    encoded = nullptr;
    EXPECT_FALSE(olib_format_write_string(OLIB_FORMAT_YAML, value, &encoded));
    EXPECT_EQ(encoded, nullptr);
    olib_free(encoded);
    olib_object_free(value);
  }
}
