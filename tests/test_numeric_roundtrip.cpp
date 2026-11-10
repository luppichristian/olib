#include "test_utils.h"

#include <cmath>
#include <limits>

TEST(NumericRoundTrip, TextFormatsPreserveFiniteDoubleValuesAndNegativeZero) {
  const auto formats = {OLIB_FORMAT_JSON_TEXT, OLIB_FORMAT_YAML, OLIB_FORMAT_XML,
                        OLIB_FORMAT_TOML, OLIB_FORMAT_TXT};
  const double values[] = {1.2345678901234567, 1e100, 1e-100, -0.0,
                           std::numeric_limits<double>::max()};
  for (auto format : formats) {
    for (double value : values) {
      SCOPED_TRACE(static_cast<int>(format));
      SCOPED_TRACE(value);
      auto* root = olib_object_new(OLIB_OBJECT_TYPE_STRUCT);
      auto* number = olib_object_new(OLIB_OBJECT_TYPE_FLOAT);
      ASSERT_NE(root, nullptr);
      ASSERT_NE(number, nullptr);
      ASSERT_TRUE(olib_object_set_float(number, value));
      ASSERT_TRUE(olib_object_struct_add(root, "value", number));
      char* encoded = nullptr;
      ASSERT_TRUE(olib_format_write_string(format, root, &encoded));
      ASSERT_NE(encoded, nullptr);
      auto* parsed = olib_format_read_string(format, encoded);
      ASSERT_NE(parsed, nullptr) << encoded;
      auto* actual = olib_object_struct_get(parsed, "value");
      ASSERT_NE(actual, nullptr) << encoded;
      EXPECT_EQ(olib_object_get_type(actual), OLIB_OBJECT_TYPE_FLOAT) << encoded;
      EXPECT_EQ(olib_object_get_float(actual), value) << encoded;
      if (value == 0.0) EXPECT_EQ(std::signbit(olib_object_get_float(actual)), std::signbit(value));
      olib_object_free(parsed);
      olib_free(encoded);
      olib_object_free(root);
    }
  }
}

TEST(NumericRoundTrip, YamlAndTextPreserveLargeUnsignedIntegers) {
  for (auto format : {OLIB_FORMAT_YAML, OLIB_FORMAT_TXT}) {
    SCOPED_TRACE(static_cast<int>(format));
    auto* root = olib_object_new(OLIB_OBJECT_TYPE_STRUCT);
    auto* value = olib_object_new(OLIB_OBJECT_TYPE_UINT);
    ASSERT_NE(root, nullptr);
    ASSERT_NE(value, nullptr);
    ASSERT_TRUE(olib_object_set_uint(value, UINT64_MAX));
    ASSERT_TRUE(olib_object_struct_add(root, "value", value));
    char* encoded = nullptr;
    ASSERT_TRUE(olib_format_write_string(format, root, &encoded));
    ASSERT_NE(encoded, nullptr);
    auto* decoded = olib_format_read_string(format, encoded);
    ASSERT_NE(decoded, nullptr) << encoded;
    auto* restored = olib_object_struct_get(decoded, "value");
    ASSERT_NE(restored, nullptr);
    EXPECT_EQ(olib_object_get_type(restored), OLIB_OBJECT_TYPE_UINT);
    EXPECT_EQ(olib_object_get_uint(restored), UINT64_MAX);
    olib_object_free(decoded);
    olib_free(encoded);
    olib_object_free(root);
  }
}

TEST(NumericRoundTrip, YamlAndTextRejectNumbersOutsideObjectRange) {
  const struct { olib_format_t format; const char* input; } inputs[] = {
      {OLIB_FORMAT_YAML, "value: 18446744073709551616"},
      {OLIB_FORMAT_YAML, "value: -9223372036854775809"},
      {OLIB_FORMAT_YAML, "value: 1e9999"},
      {OLIB_FORMAT_TXT, "{value: 18446744073709551616}"},
      {OLIB_FORMAT_TXT, "{value: -9223372036854775809}"},
      {OLIB_FORMAT_TXT, "{value: 1e9999}"},
  };
  for (const auto& input : inputs) {
    SCOPED_TRACE(input.input);
    auto* parsed = olib_format_read_string(input.format, input.input);
    EXPECT_EQ(parsed, nullptr);
    olib_object_free(parsed);
  }
}

TEST(NumericRoundTrip, TextParsersRejectIncompleteExponents) {
  const struct { olib_format_t format; const char* input; } inputs[] = {
      {OLIB_FORMAT_YAML, "value: 1e+"},
      {OLIB_FORMAT_TOML, "value = 1e+"},
      {OLIB_FORMAT_TXT, "{value: 1e+}"},
  };
  for (const auto& input : inputs) {
    SCOPED_TRACE(input.input);
    auto* parsed = olib_format_read_string(input.format, input.input);
    EXPECT_EQ(parsed, nullptr);
    olib_object_free(parsed);
  }
}

TEST(NumericRoundTrip, UnsupportedNonFiniteValuesFailWithoutOutput) {
  const double values[] = {std::numeric_limits<double>::infinity(),
                           -std::numeric_limits<double>::infinity(),
                           std::numeric_limits<double>::quiet_NaN()};
  for (auto format : {OLIB_FORMAT_JSON_TEXT, OLIB_FORMAT_YAML, OLIB_FORMAT_TOML, OLIB_FORMAT_TXT}) {
    for (double value : values) {
      SCOPED_TRACE(static_cast<int>(format));
      auto* object = olib_object_new(OLIB_OBJECT_TYPE_FLOAT);
      ASSERT_NE(object, nullptr);
      ASSERT_TRUE(olib_object_set_float(object, value));
      char* output = reinterpret_cast<char*>(1);
      EXPECT_FALSE(olib_format_write_string(format, object, &output));
      EXPECT_EQ(output, nullptr);
      if (output != reinterpret_cast<char*>(1)) olib_free(output);
      olib_object_free(object);
    }
  }
}
