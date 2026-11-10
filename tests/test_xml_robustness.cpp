#include <gtest/gtest.h>
#include <olib.h>
#include <cstring>
#include <cmath>
#include <limits>
#include <string>

TEST(XmlReading, PreservesLongAndEmptyKeys) {
  std::string long_key(512, 'k');
  long_key += "&suffix";
  auto* root = olib_object_new(OLIB_OBJECT_TYPE_STRUCT);
  auto* value = olib_object_new(OLIB_OBJECT_TYPE_INT);
  auto* empty = olib_object_new(OLIB_OBJECT_TYPE_BOOL);
  ASSERT_NE(root, nullptr);
  ASSERT_NE(value, nullptr);
  ASSERT_NE(empty, nullptr);
  ASSERT_TRUE(olib_object_set_int(value, -42));
  ASSERT_TRUE(olib_object_set_bool(empty, true));
  ASSERT_TRUE(olib_object_struct_add(root, long_key.c_str(), value));
  ASSERT_TRUE(olib_object_struct_add(root, "", empty));
  char* encoded = nullptr;
  ASSERT_TRUE(olib_format_write_string(OLIB_FORMAT_XML, root, &encoded));
  ASSERT_NE(encoded, nullptr);
  auto* decoded = olib_format_read_string(OLIB_FORMAT_XML, encoded);
  ASSERT_NE(decoded, nullptr);
  ASSERT_EQ(olib_object_struct_size(decoded), 2u);
  auto* restored = olib_object_struct_get(decoded, long_key.c_str());
  ASSERT_NE(restored, nullptr);
  EXPECT_EQ(olib_object_get_int(restored), -42);
  restored = olib_object_struct_get(decoded, "");
  ASSERT_NE(restored, nullptr);
  EXPECT_TRUE(olib_object_get_bool(restored));
  olib_object_free(decoded);
  olib_free(encoded);
  olib_object_free(root);
}

TEST(XmlWriting, NestedContainersRestoreParentContext) {
  const char* input = "{\"items\":[1,{\"text\":\"Ada & Co <x>\"},[2,3]],\"after\":7}";
  olib_object_t* object = olib_format_read_string(OLIB_FORMAT_JSON_TEXT, input);
  ASSERT_NE(object, nullptr);
  char* xml = nullptr;
  ASSERT_TRUE(olib_format_write_string(OLIB_FORMAT_XML, object, &xml));
  ASSERT_NE(xml, nullptr);
  EXPECT_NE(std::strstr(xml, "<item type=\"int\">1</item>"), nullptr);
  EXPECT_NE(std::strstr(xml, "<key name=\"text\" type=\"string\">Ada &amp; Co &lt;x&gt;</key>"), nullptr);
  EXPECT_NE(std::strstr(xml, "<item type=\"int\">2</item>"), nullptr);
  EXPECT_NE(std::strstr(xml, "<item type=\"int\">3</item>"), nullptr);
  EXPECT_NE(std::strstr(xml, "<key name=\"after\" type=\"int\">7</key>"), nullptr);
  olib_free(xml);
  olib_object_free(object);
}

TEST(XmlWriting, RejectsExcessiveNestingAndCanBeReused) {
  olib_serializer_t* serializer = olib_serializer_new_xml();
  ASSERT_NE(serializer, nullptr);
  olib_object_t* root = olib_object_new(OLIB_OBJECT_TYPE_LIST);
  ASSERT_NE(root, nullptr);
  olib_object_t* current = root;
  for (int i = 1; i < 33; ++i) {
    olib_object_t* child = olib_object_new(OLIB_OBJECT_TYPE_LIST);
    ASSERT_NE(child, nullptr);
    ASSERT_TRUE(olib_object_list_push(current, child));
    current = child;
  }
  char* output = nullptr;
  EXPECT_FALSE(olib_serializer_write_string(serializer, root, &output));
  EXPECT_EQ(output, nullptr);
  olib_free(output);
  olib_object_free(root);

  root = olib_object_new(OLIB_OBJECT_TYPE_INT);
  ASSERT_NE(root, nullptr);
  ASSERT_TRUE(olib_object_set_int(root, 42));
  ASSERT_TRUE(olib_serializer_write_string(serializer, root, &output));
  EXPECT_NE(std::strstr(output, "<int>42</int>"), nullptr);
  olib_free(output);
  olib_object_free(root);
  olib_serializer_free(serializer);
}

TEST(XmlWriting, PreservesFiniteFloatPrecisionAndSign) {
  for (double value : {1.2345678901234567, -0.0}) {
    olib_object_t* object = olib_object_new(OLIB_OBJECT_TYPE_FLOAT);
    ASSERT_NE(object, nullptr);
    ASSERT_TRUE(olib_object_set_float(object, value));
    char* encoded = nullptr;
    ASSERT_TRUE(olib_format_write_string(OLIB_FORMAT_XML, object, &encoded));
    olib_object_t* decoded = olib_format_read_string(OLIB_FORMAT_XML, encoded);
    ASSERT_NE(decoded, nullptr);
    ASSERT_EQ(olib_object_get_type(decoded), OLIB_OBJECT_TYPE_FLOAT);
    EXPECT_DOUBLE_EQ(olib_object_get_float(decoded), value);
    EXPECT_EQ(std::signbit(olib_object_get_float(decoded)), std::signbit(value));
    olib_object_free(decoded);
    olib_free(encoded);
    olib_object_free(object);
  }
}

TEST(XmlWriting, RejectsInvalidXmlCharactersAndUtf8InValuesAndKeys) {
  const char* invalid[] = {"A\x01" "B", "\x0b", "\xc0\x80", "\xe2\x82",
                           "\xed\xa0\x80", "\xef\xbf\xbe", "\xf4\x90\x80\x80"};
  for (const char* text : invalid) {
    SCOPED_TRACE(testing::PrintToString(std::string(text)));
    auto* value = olib_object_new(OLIB_OBJECT_TYPE_STRING);
    ASSERT_NE(value, nullptr);
    ASSERT_TRUE(olib_object_set_string(value, text));
    char* encoded = nullptr;
    EXPECT_FALSE(olib_format_write_string(OLIB_FORMAT_XML, value, &encoded));
    EXPECT_EQ(encoded, nullptr);
    olib_free(encoded);
    olib_object_free(value);

    auto* root = olib_object_new(OLIB_OBJECT_TYPE_STRUCT);
    value = olib_object_new(OLIB_OBJECT_TYPE_INT);
    ASSERT_NE(root, nullptr);
    ASSERT_NE(value, nullptr);
    ASSERT_TRUE(olib_object_struct_add(root, text, value));
    encoded = nullptr;
    EXPECT_FALSE(olib_format_write_string(OLIB_FORMAT_XML, root, &encoded));
    EXPECT_EQ(encoded, nullptr);
    olib_free(encoded);
    olib_object_free(root);
  }
}

TEST(XmlReading, RejectsInvalidXmlCharactersAndUtf8) {
  const char* invalid[] = {"A\x01" "B", "\x0b", "\xc0\x80", "\xe2\x82",
                           "\xed\xa0\x80", "\xef\xbf\xbf", "\xf4\x90\x80\x80"};
  for (const char* text : invalid) {
    std::string input = std::string("<olib><string>") + text + "</string></olib>";
    SCOPED_TRACE(testing::PrintToString(input));
    auto* parsed = olib_format_read_string(OLIB_FORMAT_XML, input.c_str());
    EXPECT_EQ(parsed, nullptr);
    olib_object_free(parsed);
  }
}

TEST(XmlWriting, AllowsXmlWhitespaceAndValidUnicode) {
  const char* text = "tabs\tline\nreturn\r\xc2\x85\xef\xbf\xbd\xf0\x9f\x8c\x8d";
  auto* value = olib_object_new(OLIB_OBJECT_TYPE_STRING);
  ASSERT_NE(value, nullptr);
  ASSERT_TRUE(olib_object_set_string(value, text));
  char* encoded = nullptr;
  ASSERT_TRUE(olib_format_write_string(OLIB_FORMAT_XML, value, &encoded));
  auto* parsed = olib_format_read_string(OLIB_FORMAT_XML, encoded);
  ASSERT_NE(parsed, nullptr);
  ASSERT_EQ(olib_object_get_type(parsed), OLIB_OBJECT_TYPE_STRING);
  EXPECT_STREQ(olib_object_get_string(parsed), text);
  olib_object_free(parsed);
  olib_free(encoded);
  olib_object_free(value);
}

TEST(XmlWriting, EscapesWhitespaceThatXmlParsersNormalize) {
  const char* key = "key\t\n\rvalue";
  const char* text = "text\t\n\rvalue";
  auto* root = olib_object_new(OLIB_OBJECT_TYPE_STRUCT);
  auto* value = olib_object_new(OLIB_OBJECT_TYPE_STRING);
  ASSERT_NE(root, nullptr);
  ASSERT_NE(value, nullptr);
  ASSERT_TRUE(olib_object_set_string(value, text));
  ASSERT_TRUE(olib_object_struct_add(root, key, value));
  char* encoded = nullptr;
  ASSERT_TRUE(olib_format_write_string(OLIB_FORMAT_XML, root, &encoded));
  ASSERT_NE(encoded, nullptr);
  EXPECT_NE(std::strstr(encoded, "key&#9;&#10;&#13;value"), nullptr);
  EXPECT_NE(std::strstr(encoded, "text\t\n&#13;value"), nullptr);
  auto* decoded = olib_format_read_string(OLIB_FORMAT_XML, encoded);
  ASSERT_NE(decoded, nullptr);
  auto* field = olib_object_struct_get(decoded, key);
  ASSERT_NE(field, nullptr);
  EXPECT_STREQ(olib_object_get_string(field), text);
  olib_object_free(decoded);
  olib_free(encoded);
  olib_object_free(root);
}

TEST(XmlReading, DecodesDecimalAndHexadecimalCharacterReferences) {
  const char* xml = "<olib><struct><key name=\"a&#9;&#xA;&#13;b\" type=\"string\">"
                    "x&#x9;&#10;&#xD;y &#x1F30D;</key></struct></olib>";
  auto* root = olib_format_read_string(OLIB_FORMAT_XML, xml);
  ASSERT_NE(root, nullptr);
  auto* field = olib_object_struct_get(root, "a\t\n\rb");
  ASSERT_NE(field, nullptr);
  EXPECT_STREQ(olib_object_get_string(field), "x\t\n\ry \xf0\x9f\x8c\x8d");
  olib_object_free(root);
}

TEST(XmlReading, RejectsInvalidCharacterReferences) {
  for (const char* entity : {"&#0;", "&#xD800;", "&#xFFFE;", "&#x110000;", "&#999999999999999999;", "&#x;", "&#12", "&unknown;"}) {
    std::string xml = std::string("<olib><string>") + entity + "</string></olib>";
    SCOPED_TRACE(entity);
    auto* parsed = olib_format_read_string(OLIB_FORMAT_XML, xml.c_str());
    EXPECT_EQ(parsed, nullptr);
    olib_object_free(parsed);
  }
}

TEST(XmlReading, PreservesNumericBoundariesAndLongWhitespace) {
  const struct { const char* input; int64_t expected; } signed_cases[] = {
      {"-9223372036854775808", INT64_MIN}, {"9223372036854775807", INT64_MAX},
      {" \t+42\r\n", 42}};
  for (const auto& test : signed_cases) {
    std::string xml = std::string("<olib><int>") + test.input + "</int></olib>";
    auto* object = olib_format_read_string(OLIB_FORMAT_XML, xml.c_str());
    ASSERT_NE(object, nullptr) << test.input;
    EXPECT_EQ(olib_object_get_type(object), OLIB_OBJECT_TYPE_INT);
    EXPECT_EQ(olib_object_get_int(object), test.expected);
    olib_object_free(object);
  }
  const char* unsigned_xml = "<olib><uint>18446744073709551615</uint></olib>";
  auto* object = olib_format_read_string(OLIB_FORMAT_XML, unsigned_xml);
  ASSERT_NE(object, nullptr);
  EXPECT_EQ(olib_object_get_type(object), OLIB_OBJECT_TYPE_UINT);
  EXPECT_EQ(olib_object_get_uint(object), UINT64_MAX);
  olib_object_free(object);

  std::string long_xml = "<olib><int>" + std::string(300, ' ') + "42" +
                         std::string(300, ' ') + "</int></olib>";
  object = olib_format_read_string(OLIB_FORMAT_XML, long_xml.c_str());
  ASSERT_NE(object, nullptr);
  EXPECT_EQ(olib_object_get_int(object), 42);
  olib_object_free(object);

  object = olib_format_read_string(OLIB_FORMAT_XML, "<olib><float>4.9406564584124654e-324</float></olib>");
  ASSERT_NE(object, nullptr);
  EXPECT_EQ(olib_object_get_type(object), OLIB_OBJECT_TYPE_FLOAT);
  EXPECT_EQ(olib_object_get_float(object), std::numeric_limits<double>::denorm_min());
  olib_object_free(object);
}

TEST(XmlReading, RejectsMalformedAndOutOfRangeNumbers) {
  const struct { const char* type; const char* input; } cases[] = {
      {"int", "abc"}, {"int", "12junk"}, {"int", ""},
      {"int", "9223372036854775808"}, {"int", "-9223372036854775809"},
      {"uint", "-1"}, {"uint", "18446744073709551616"}, {"uint", "1.5"},
      {"float", "1e+"}, {"float", "12junk"}, {"float", "1e9999"},
      {"float", "1e-9999"}, {"float", "  "}};
  for (const auto& test : cases) {
    SCOPED_TRACE(test.input);
    std::string xml = std::string("<olib><") + test.type + ">" + test.input +
                      "</" + test.type + "></olib>";
    auto* object = olib_format_read_string(OLIB_FORMAT_XML, xml.c_str());
    EXPECT_EQ(object, nullptr);
    olib_object_free(object);
  }
}

TEST(XmlReading, ParsesBooleanSpellingsAndWhitespace) {
  const struct { const char* input; bool expected; } cases[] = {
      {"true", true}, {"1", true}, {"false", false}, {"0", false},
      {" \ttrue\r\n", true}, {" \n0\t", false}};
  for (const auto& test : cases) {
    SCOPED_TRACE(test.input);
    std::string xml = std::string("<olib><bool>") + test.input + "</bool></olib>";
    auto* object = olib_format_read_string(OLIB_FORMAT_XML, xml.c_str());
    ASSERT_NE(object, nullptr);
    EXPECT_EQ(olib_object_get_type(object), OLIB_OBJECT_TYPE_BOOL);
    EXPECT_EQ(olib_object_get_bool(object), test.expected);
    olib_object_free(object);
  }
  std::string xml = "<olib><bool>" + std::string(200, ' ') + "true" +
                    std::string(200, ' ') + "</bool></olib>";
  auto* object = olib_format_read_string(OLIB_FORMAT_XML, xml.c_str());
  ASSERT_NE(object, nullptr);
  EXPECT_TRUE(olib_object_get_bool(object));
  olib_object_free(object);
}

TEST(XmlReading, RejectsEmptyAndMalformedBooleans) {
  for (const char* text : {"", " \t\n", "False", "falsehood", "2", "-1", "truth"}) {
    SCOPED_TRACE(text);
    std::string xml = std::string("<olib><bool>") + text + "</bool></olib>";
    auto* object = olib_format_read_string(OLIB_FORMAT_XML, xml.c_str());
    EXPECT_EQ(object, nullptr);
    olib_object_free(object);
  }
}
