#include <gtest/gtest.h>
#include <olib.h>
#include <cmath>
#include <limits>

static void expect_equal_tree(olib_object_t* actual, olib_object_t* expected) {
  ASSERT_NE(actual, nullptr);
  ASSERT_NE(expected, nullptr);
  ASSERT_EQ(olib_object_get_type(actual), olib_object_get_type(expected));
  switch (olib_object_get_type(expected)) {
    case OLIB_OBJECT_TYPE_STRUCT:
      ASSERT_EQ(olib_object_struct_size(actual), olib_object_struct_size(expected));
      for (size_t i = 0; i < olib_object_struct_size(expected); ++i) {
        const char* key = olib_object_struct_key_at(expected, i);
        SCOPED_TRACE(key);
        expect_equal_tree(olib_object_struct_get(actual, key), olib_object_struct_value_at(expected, i));
      }
      break;
    case OLIB_OBJECT_TYPE_LIST:
      ASSERT_EQ(olib_object_list_size(actual), olib_object_list_size(expected));
      for (size_t i = 0; i < olib_object_list_size(expected); ++i) {
        SCOPED_TRACE(i);
        expect_equal_tree(olib_object_list_get(actual, i), olib_object_list_get(expected, i));
      }
      break;
    case OLIB_OBJECT_TYPE_INT:
      EXPECT_EQ(olib_object_get_int(actual), olib_object_get_int(expected));
      break;
    case OLIB_OBJECT_TYPE_STRING:
      EXPECT_STREQ(olib_object_get_string(actual), olib_object_get_string(expected));
      break;
    case OLIB_OBJECT_TYPE_BOOL:
      EXPECT_EQ(olib_object_get_bool(actual), olib_object_get_bool(expected));
      break;
    default:
      FAIL() << "Unexpected test fixture type";
  }
}

TEST(TomlNested, MixedArrayContainingInlineTableRoundTrips) {
  auto* object = olib_format_read_string(OLIB_FORMAT_JSON_TEXT,
      R"({"name":"example","values":[1,2,{"inner":"longer value","active":true},3],"enabled":true})");
  ASSERT_NE(object, nullptr);
  char* encoded = nullptr;
  ASSERT_TRUE(olib_format_write_string(OLIB_FORMAT_TOML, object, &encoded));
  ASSERT_NE(encoded, nullptr);
  auto* decoded = olib_format_read_string(OLIB_FORMAT_TOML, encoded);
  ASSERT_NE(decoded, nullptr) << encoded;
  auto* values = olib_object_struct_get(decoded, "values");
  ASSERT_EQ(olib_object_list_size(values), 4u) << encoded;
  EXPECT_EQ(olib_object_get_int(olib_object_list_get(values, 0)), 1);
  EXPECT_EQ(olib_object_get_int(olib_object_list_get(values, 1)), 2);
  auto* nested = olib_object_list_get(values, 2);
  ASSERT_NE(nested, nullptr);
  EXPECT_STREQ(olib_object_get_string(olib_object_struct_get(nested, "inner")), "longer value");
  EXPECT_TRUE(olib_object_get_bool(olib_object_struct_get(nested, "active")));
  EXPECT_EQ(olib_object_get_int(olib_object_list_get(values, 3)), 3);
  EXPECT_TRUE(olib_object_get_bool(olib_object_struct_get(decoded, "enabled")));
  olib_object_free(decoded);
  olib_free(encoded);
  olib_object_free(object);
}

TEST(TomlNested, NestedArraysAndInlineTablesPreserveSiblings) {
  auto* object = olib_format_read_string(OLIB_FORMAT_JSON_TEXT,
      R"({"data":[[1,2],[3,[4]],{"values":[5,6],"nested":{"key":"val"},"tail":7},9],"after":true})");
  ASSERT_NE(object, nullptr);
  char* encoded = nullptr;
  ASSERT_TRUE(olib_format_write_string(OLIB_FORMAT_TOML, object, &encoded));
  ASSERT_NE(encoded, nullptr);
  auto* decoded = olib_format_read_string(OLIB_FORMAT_TOML, encoded);
  ASSERT_NE(decoded, nullptr) << encoded;
  SCOPED_TRACE(encoded);
  expect_equal_tree(decoded, object);
  olib_object_free(decoded);
  olib_free(encoded);
  olib_object_free(object);
}

TEST(TomlNested, FloatingPointValuesPreservePrecisionAndNegativeZero) {
  const double values[] = {1.0000000000000002, -0.0, 3.141592653589793};
  for (double expected : values) {
    auto* object = olib_object_new(OLIB_OBJECT_TYPE_STRUCT);
    auto* value = olib_object_new(OLIB_OBJECT_TYPE_FLOAT);
    ASSERT_NE(object, nullptr);
    ASSERT_NE(value, nullptr);
    ASSERT_TRUE(olib_object_set_float(value, expected));
    ASSERT_TRUE(olib_object_struct_add(object, "value", value));
    char* encoded = nullptr;
    ASSERT_TRUE(olib_format_write_string(OLIB_FORMAT_TOML, object, &encoded));
    ASSERT_NE(encoded, nullptr);
    auto* decoded = olib_format_read_string(OLIB_FORMAT_TOML, encoded);
    ASSERT_NE(decoded, nullptr) << encoded;
    auto* result = olib_object_struct_get(decoded, "value");
    ASSERT_NE(result, nullptr);
    ASSERT_EQ(olib_object_get_type(result), OLIB_OBJECT_TYPE_FLOAT);
    EXPECT_EQ(olib_object_get_float(result), expected) << encoded;
    EXPECT_EQ(std::signbit(olib_object_get_float(result)), std::signbit(expected)) << encoded;
    olib_object_free(decoded);
    olib_free(encoded);
    olib_object_free(object);
  }
}

TEST(TomlNested, QuotedRootKeysRoundTripIncludingEmptyAndControlKeys) {
  auto* object = olib_object_new(OLIB_OBJECT_TYPE_STRUCT);
  ASSERT_NE(object, nullptr);
  const char* keys[] = {"spaced key", "", "line\x01" "break", "123", "has-hyphen"};
  for (const char* key : keys) {
    auto* value = olib_object_new(OLIB_OBJECT_TYPE_STRING);
    ASSERT_NE(value, nullptr);
    ASSERT_TRUE(olib_object_set_string(value, "ok"));
    ASSERT_TRUE(olib_object_struct_add(object, key, value));
  }
  char* encoded = nullptr;
  ASSERT_TRUE(olib_format_write_string(OLIB_FORMAT_TOML, object, &encoded));
  ASSERT_NE(encoded, nullptr);
  auto* decoded = olib_format_read_string(OLIB_FORMAT_TOML, encoded);
  ASSERT_NE(decoded, nullptr) << encoded;
  ASSERT_EQ(olib_object_get_type(decoded), OLIB_OBJECT_TYPE_STRUCT) << encoded;
  ASSERT_EQ(olib_object_struct_size(decoded), 5u);
  for (const char* key : keys) {
    auto* value = olib_object_struct_get(decoded, key);
    ASSERT_NE(value, nullptr) << key;
    EXPECT_STREQ(olib_object_get_string(value), "ok");
  }
  olib_object_free(decoded);
  olib_free(encoded);
  olib_object_free(object);
}

TEST(TomlNested, UnicodeEscapesAndControlCharactersRoundTrip) {
  auto* object = olib_object_new(OLIB_OBJECT_TYPE_STRUCT);
  auto* value = olib_object_new(OLIB_OBJECT_TYPE_STRING);
  ASSERT_NE(object, nullptr);
  ASSERT_NE(value, nullptr);
  ASSERT_TRUE(olib_object_set_string(value, "before\x01\x1f" "after"));
  ASSERT_TRUE(olib_object_struct_add(object, "value", value));
  char* encoded = nullptr;
  ASSERT_TRUE(olib_format_write_string(OLIB_FORMAT_TOML, object, &encoded));
  auto* decoded = olib_format_read_string(OLIB_FORMAT_TOML, encoded);
  ASSERT_NE(decoded, nullptr);
  auto* result = olib_object_struct_get(decoded, "value");
  ASSERT_NE(result, nullptr);
  EXPECT_STREQ(olib_object_get_string(result), "before\x01\x1f" "after");
  olib_object_free(decoded);
  olib_free(encoded);
  olib_object_free(object);

  decoded = olib_format_read_string(OLIB_FORMAT_TOML,
                                    "value = \"\\u00e9\\U0001f30d\"\n");
  ASSERT_NE(decoded, nullptr);
  result = olib_object_struct_get(decoded, "value");
  ASSERT_NE(result, nullptr);
  EXPECT_STREQ(olib_object_get_string(result), "\xc3\xa9\xf0\x9f\x8c\x8d");
  olib_object_free(decoded);

  for (const char* input : {"value = \"\\u0000\"\n", "value = \"\\u12xz\"\n",
                            "value = \"\\ud800\"\n", "value = \"\\U00110000\"\n"}) {
    decoded = olib_format_read_string(OLIB_FORMAT_TOML, input);
    EXPECT_EQ(decoded, nullptr) << input;
    olib_object_free(decoded);
  }
}

TEST(TomlNested, PreservesUint64MaxAndRejectsUnrepresentableIntegers) {
  auto* root = olib_object_new(OLIB_OBJECT_TYPE_STRUCT);
  auto* value = olib_object_new(OLIB_OBJECT_TYPE_UINT);
  ASSERT_NE(root, nullptr);
  ASSERT_NE(value, nullptr);
  ASSERT_TRUE(olib_object_set_uint(value, UINT64_MAX));
  ASSERT_TRUE(olib_object_struct_add(root, "value", value));
  char* encoded = nullptr;
  ASSERT_TRUE(olib_format_write_string(OLIB_FORMAT_TOML, root, &encoded));
  auto* roundtrip = olib_format_read_string(OLIB_FORMAT_TOML, encoded);
  ASSERT_NE(roundtrip, nullptr);
  auto* result = olib_object_struct_get(roundtrip, "value");
  ASSERT_NE(result, nullptr);
  EXPECT_EQ(olib_object_get_type(result), OLIB_OBJECT_TYPE_UINT);
  EXPECT_EQ(olib_object_get_uint(result), UINT64_MAX);
  olib_object_free(roundtrip);
  olib_free(encoded);
  olib_object_free(root);

  for (const char* input : {"value = 18446744073709551616\n",
                            "value = -9223372036854775809\n"}) {
    auto* parsed = olib_format_read_string(OLIB_FORMAT_TOML, input);
    EXPECT_EQ(parsed, nullptr) << input;
    olib_object_free(parsed);
  }
}

TEST(TomlNested, RejectsNonFiniteNumbersWithoutProducingOutput) {
  const double values[] = {std::numeric_limits<double>::infinity(),
                           -std::numeric_limits<double>::infinity(),
                           std::numeric_limits<double>::quiet_NaN()};
  for (double value : values) {
    auto* number = olib_object_new(OLIB_OBJECT_TYPE_FLOAT);
    ASSERT_NE(number, nullptr);
    ASSERT_TRUE(olib_object_set_float(number, value));
    char* encoded = nullptr;
    EXPECT_FALSE(olib_format_write_string(OLIB_FORMAT_TOML, number, &encoded));
    EXPECT_EQ(encoded, nullptr);
    olib_free(encoded);
    olib_object_free(number);
  }
}
