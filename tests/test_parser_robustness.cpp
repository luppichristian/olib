#include <gtest/gtest.h>
#include <olib.h>
#include <string>
#include <limits>
#include <cmath>

TEST(JsonParsing, DecodesUnicodeEscapesAndSurrogatePairs) {
  const char* input = "\"\\u0041\\u00e9\\u4e16\\ud83c\\udf0d\"";
  olib_object_t* object = olib_format_read_string(OLIB_FORMAT_JSON_TEXT, input);
  ASSERT_NE(object, nullptr);
  EXPECT_EQ(olib_object_get_type(object), OLIB_OBJECT_TYPE_STRING);
  EXPECT_STREQ(olib_object_get_string(object), "A\xc3\xa9\xe4\xb8\x96\xf0\x9f\x8c\x8d");
  olib_object_free(object);
}

TEST(JsonParsing, DecodesManyMultibyteEscapes) {
  std::string input = "\"";
  std::string expected;
  for (int i = 0; i < 100; ++i) {
    input += "\\u0800";
    expected += "\xe0\xa0\x80";
  }
  input += "\"";
  olib_object_t* object = olib_format_read_string(OLIB_FORMAT_JSON_TEXT, input.c_str());
  ASSERT_NE(object, nullptr);
  EXPECT_EQ(olib_object_get_type(object), OLIB_OBJECT_TYPE_STRING);
  EXPECT_STREQ(olib_object_get_string(object), expected.c_str());
  olib_object_free(object);
}

TEST(JsonParsing, RejectsInvalidStringEscapesAndControlCharacters) {
  const char* inputs[] = {
      "\"\\x\"", "\"\\u12g4\"", "\"\\u123\"", "\"\\ud800\"",
      "\"\\udc00\"", "\"\\ud800\\u0041\"", "\"\\u0000\"", "\"line\nbreak\"",
  };
  for (const char* input : inputs) {
    olib_object_t* object = olib_format_read_string(OLIB_FORMAT_JSON_TEXT, input);
    EXPECT_EQ(object, nullptr) << input;
    olib_object_free(object);
  }
}

TEST(JsonParsing, RejectsInvalidUtf8InStrings) {
  const char* inputs[] = {
      "\"\xc0\x80\"", "\"\xe2\x28\xa1\"", "\"\xed\xa0\x80\"", "\"\xf4\x90\x80\x80\"",
      "\"\xe2\x82\"", "\"\x80\"",
  };
  for (const char* input : inputs) {
    olib_object_t* object = olib_format_read_string(OLIB_FORMAT_JSON_TEXT, input);
    EXPECT_EQ(object, nullptr);
    olib_object_free(object);
  }
  olib_object_t* valid = olib_format_read_string(OLIB_FORMAT_JSON_TEXT, "\"\xe4\xb8\x96\xf0\x9f\x8c\x8d\"");
  ASSERT_NE(valid, nullptr);
  EXPECT_STREQ(olib_object_get_string(valid), "\xe4\xb8\x96\xf0\x9f\x8c\x8d");
  olib_object_free(valid);
}

TEST(JsonWriting, RejectsInvalidUtf8InStrings) {
  olib_object_t* object = olib_object_new(OLIB_OBJECT_TYPE_STRING);
  ASSERT_NE(object, nullptr);
  ASSERT_TRUE(olib_object_set_string(object, "\xc0\x80"));
  char* encoded = nullptr;
  EXPECT_FALSE(olib_format_write_string(OLIB_FORMAT_JSON_TEXT, object, &encoded));
  EXPECT_EQ(encoded, nullptr);
  olib_free(encoded);
  olib_object_free(object);
}

TEST(JsonWriting, RejectsNonFiniteValues) {
  const double values[] = {std::numeric_limits<double>::quiet_NaN(),
                           std::numeric_limits<double>::infinity(),
                           -std::numeric_limits<double>::infinity()};
  for (double value : values) {
    olib_object_t* object = olib_object_new(OLIB_OBJECT_TYPE_FLOAT);
    ASSERT_NE(object, nullptr);
    ASSERT_TRUE(olib_object_set_float(object, value));
    char* encoded = nullptr;
    EXPECT_FALSE(olib_format_write_string(OLIB_FORMAT_JSON_TEXT, object, &encoded));
    EXPECT_EQ(encoded, nullptr);
    olib_free(encoded);
    olib_object_free(object);
  }
}

TEST(JsonWriting, RoundTripsFinitePrecisionAndNegativeZero) {
  for (double value : {1.2345678901234567, -0.0}) {
    olib_object_t* object = olib_object_new(OLIB_OBJECT_TYPE_FLOAT);
    ASSERT_NE(object, nullptr);
    ASSERT_TRUE(olib_object_set_float(object, value));
    char* encoded = nullptr;
    ASSERT_TRUE(olib_format_write_string(OLIB_FORMAT_JSON_TEXT, object, &encoded));
    olib_object_t* decoded = olib_format_read_string(OLIB_FORMAT_JSON_TEXT, encoded);
    ASSERT_NE(decoded, nullptr);
    ASSERT_EQ(olib_object_get_type(decoded), OLIB_OBJECT_TYPE_FLOAT);
    EXPECT_DOUBLE_EQ(olib_object_get_float(decoded), value);
    EXPECT_EQ(std::signbit(olib_object_get_float(decoded)), std::signbit(value));
    olib_object_free(decoded);
    olib_free(encoded);
    olib_object_free(object);
  }
}

TEST(JsonParsing, RejectsInvalidNumberGrammar) {
  const char* inputs[] = {"01", "-01", "1.", "1e", "1e+", "1e-"};
  for (const char* input : inputs) {
    olib_object_t* object = olib_format_read_string(OLIB_FORMAT_JSON_TEXT, input);
    EXPECT_EQ(object, nullptr) << input;
    olib_object_free(object);
  }
}

TEST(JsonParsing, PreservesIntegerBoundariesAndRejectsOverflow) {
  struct Case { const char* text; olib_object_type_t type; int64_t signed_value; uint64_t unsigned_value; };
  const Case cases[] = {
      {"-9223372036854775808", OLIB_OBJECT_TYPE_INT, INT64_MIN, 0},
      {"9223372036854775807", OLIB_OBJECT_TYPE_INT, INT64_MAX, 0},
      {"18446744073709551615", OLIB_OBJECT_TYPE_UINT, 0, UINT64_MAX},
  };
  for (const auto& test : cases) {
    olib_object_t* object = olib_format_read_string(OLIB_FORMAT_JSON_TEXT, test.text);
    ASSERT_NE(object, nullptr) << test.text;
    EXPECT_EQ(olib_object_get_type(object), test.type);
    if (test.type == OLIB_OBJECT_TYPE_INT) EXPECT_EQ(olib_object_get_int(object), test.signed_value);
    else EXPECT_EQ(olib_object_get_uint(object), test.unsigned_value);
    olib_object_free(object);
  }
  const char* invalid[] = {"-9223372036854775809", "18446744073709551616", "1e400", "1e-400"};
  for (const char* input : invalid) {
    olib_object_t* object = olib_format_read_string(OLIB_FORMAT_JSON_TEXT, input);
    EXPECT_EQ(object, nullptr) << input;
    olib_object_free(object);
  }
}

TEST(JsonParsing, RejectsMissingAndTrailingObjectCommas) {
  const char* inputs[] = {"{\"a\": 1 \"b\": 2}", "{\"a\": 1,}", "{,\"a\": 1}",
                          "{\"a\":,1}", ",1"};
  for (const char* input : inputs) {
    olib_object_t* object = olib_format_read_string(OLIB_FORMAT_JSON_TEXT, input);
    EXPECT_EQ(object, nullptr) << input;
    olib_object_free(object);
  }
}

TEST(JsonParsing, RejectsInvalidArraySeparators) {
  const char* inputs[] = {"[,1 2]", "[1 2,]", "[1,2 3,]", "[1,]", "[,1]", "[1,,2]"};
  for (const char* input : inputs) {
    SCOPED_TRACE(input);
    auto* parsed = olib_format_read_string(OLIB_FORMAT_JSON_TEXT, input);
    EXPECT_EQ(parsed, nullptr);
    olib_object_free(parsed);
  }
}

TEST(JsonParsing, RejectsUnrepresentableNullRatherThanConvertingItToZero) {
  for (const char* input : {"null", "[null]", "{\"value\":null}"}) {
    SCOPED_TRACE(input);
    auto* parsed = olib_format_read_string(OLIB_FORMAT_JSON_TEXT, input);
    EXPECT_EQ(parsed, nullptr);
    olib_object_free(parsed);
  }
}

TEST(JsonParsing, DecodesUnicodeObjectKeys) {
  olib_object_t* object = olib_format_read_string(OLIB_FORMAT_JSON_TEXT, "{\"\\ud83c\\udf0d\": 7}");
  ASSERT_NE(object, nullptr);
  olib_object_t* value = olib_object_struct_get(object, "\xf0\x9f\x8c\x8d");
  ASSERT_NE(value, nullptr);
  EXPECT_EQ(olib_object_get_int(value), 7);
  olib_object_free(object);
}

TEST(JsonParsing, RejectsNestedContainersPastDepthLimit) {
  std::string input(65, '[');
  input += '0';
  input.append(65, ']');
  olib_object_t* object = olib_format_read_string(OLIB_FORMAT_JSON_TEXT, input.c_str());
  EXPECT_EQ(object, nullptr);
  olib_object_free(object);
}

static olib_object_t* nested_lists(int depth) {
  olib_object_t* root = olib_object_new(OLIB_OBJECT_TYPE_LIST);
  if (!root) return nullptr;
  olib_object_t* current = root;
  for (int i = 1; i < depth; ++i) {
    olib_object_t* child = olib_object_new(OLIB_OBJECT_TYPE_LIST);
    if (!child || !olib_object_list_push(current, child)) {
      olib_object_free(child);
      olib_object_free(root);
      return nullptr;
    }
    current = child;
  }
  return root;
}

TEST(JsonWriting, RejectsDepthBeyondCapacityAndCanBeReused) {
  olib_serializer_t* serializer = olib_serializer_new_json_text();
  ASSERT_NE(serializer, nullptr);
  olib_object_t* too_deep = nested_lists(65);
  ASSERT_NE(too_deep, nullptr);
  char* output = reinterpret_cast<char*>(1);
  EXPECT_FALSE(olib_serializer_write_string(serializer, too_deep, &output));
  EXPECT_EQ(output, nullptr);
  olib_free(output);
  olib_object_free(too_deep);

  olib_object_t* at_limit = nested_lists(64);
  ASSERT_NE(at_limit, nullptr);
  ASSERT_TRUE(olib_serializer_write_string(serializer, at_limit, &output));
  ASSERT_NE(output, nullptr);
  olib_object_t* parsed = olib_serializer_read_string(serializer, output);
  ASSERT_NE(parsed, nullptr);
  olib_object_free(parsed);
  olib_free(output);
  olib_object_free(at_limit);
  olib_serializer_free(serializer);
}
