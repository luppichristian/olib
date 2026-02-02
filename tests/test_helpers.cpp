#include "test_utils.h"

// =============================================================================
// Format Serializer Factory Tests
// =============================================================================

TEST(Helpers, FormatSerializer) {
  olib_serializer_t* json = olib_format_serializer(OLIB_FORMAT_JSON_TEXT);
  ASSERT_NE(json, nullptr);
  olib_serializer_free(json);

  olib_serializer_t* yaml = olib_format_serializer(OLIB_FORMAT_YAML);
  ASSERT_NE(yaml, nullptr);
  olib_serializer_free(yaml);

  olib_serializer_t* xml = olib_format_serializer(OLIB_FORMAT_XML);
  ASSERT_NE(xml, nullptr);
  olib_serializer_free(xml);

  olib_serializer_t* binary = olib_format_serializer(OLIB_FORMAT_BINARY);
  ASSERT_NE(binary, nullptr);
  olib_serializer_free(binary);

  olib_serializer_t* toml = olib_format_serializer(OLIB_FORMAT_TOML);
  ASSERT_NE(toml, nullptr);
  olib_serializer_free(toml);

  olib_serializer_t* txt = olib_format_serializer(OLIB_FORMAT_TXT);
  ASSERT_NE(txt, nullptr);
  olib_serializer_free(txt);

  olib_serializer_t* json_binary = olib_format_serializer(OLIB_FORMAT_JSON_BINARY);
  ASSERT_NE(json_binary, nullptr);
  olib_serializer_free(json_binary);
}

// =============================================================================
// Format Write/Read String Tests
// =============================================================================

TEST(Helpers, FormatWriteReadString) {
  olib_object_t* original = create_test_object();

  char* str = nullptr;
  EXPECT_TRUE(olib_format_write_string(OLIB_FORMAT_JSON_TEXT, original, &str));
  ASSERT_NE(str, nullptr);

  olib_object_t* parsed = olib_format_read_string(OLIB_FORMAT_JSON_TEXT, str);
  verify_test_object(parsed);

  olib_free(str);
  olib_object_free(original);
  olib_object_free(parsed);
}

// =============================================================================
// Format Write/Read Binary Tests
// =============================================================================

TEST(Helpers, FormatWriteReadBinary) {
  olib_object_t* original = create_test_object();

  uint8_t* data = nullptr;
  size_t size = 0;
  EXPECT_TRUE(olib_format_write(OLIB_FORMAT_BINARY, original, &data, &size));
  ASSERT_NE(data, nullptr);

  olib_object_t* parsed = olib_format_read(OLIB_FORMAT_BINARY, data, size);
  verify_test_object(parsed);

  olib_free(data);
  olib_object_free(original);
  olib_object_free(parsed);
}

// =============================================================================
// Format Conversion Tests
// =============================================================================

TEST(Conversion, JsonToYaml) {
  olib_object_t* original = create_test_object();

  char* json = nullptr;
  EXPECT_TRUE(olib_format_write_string(OLIB_FORMAT_JSON_TEXT, original, &json));

  char* yaml = nullptr;
  EXPECT_TRUE(olib_convert_string(OLIB_FORMAT_JSON_TEXT, json, OLIB_FORMAT_YAML, &yaml));
  ASSERT_NE(yaml, nullptr);

  olib_object_t* parsed = olib_format_read_string(OLIB_FORMAT_YAML, yaml);
  verify_test_object(parsed);

  olib_free(json);
  olib_free(yaml);
  olib_object_free(original);
  olib_object_free(parsed);
}

TEST(Conversion, YamlToXml) {
  olib_object_t* original = create_test_object();

  char* yaml = nullptr;
  EXPECT_TRUE(olib_format_write_string(OLIB_FORMAT_YAML, original, &yaml));

  char* xml = nullptr;
  EXPECT_TRUE(olib_convert_string(OLIB_FORMAT_YAML, yaml, OLIB_FORMAT_XML, &xml));
  ASSERT_NE(xml, nullptr);

  olib_object_t* parsed = olib_format_read_string(OLIB_FORMAT_XML, xml);
  verify_test_object(parsed);

  olib_free(yaml);
  olib_free(xml);
  olib_object_free(original);
  olib_object_free(parsed);
}

TEST(Conversion, XmlToToml) {
  olib_object_t* original = create_test_object();

  char* xml = nullptr;
  EXPECT_TRUE(olib_format_write_string(OLIB_FORMAT_XML, original, &xml));

  char* toml = nullptr;
  EXPECT_TRUE(olib_convert_string(OLIB_FORMAT_XML, xml, OLIB_FORMAT_TOML, &toml));
  ASSERT_NE(toml, nullptr);

  olib_object_t* parsed = olib_format_read_string(OLIB_FORMAT_TOML, toml);
  verify_test_object(parsed);

  olib_free(xml);
  olib_free(toml);
  olib_object_free(original);
  olib_object_free(parsed);
}

TEST(Conversion, TomlToBinary) {
  olib_object_t* original = create_test_object();

  char* toml = nullptr;
  EXPECT_TRUE(olib_format_write_string(OLIB_FORMAT_TOML, original, &toml));

  uint8_t* binary = nullptr;
  size_t size = 0;
  EXPECT_TRUE(olib_convert(OLIB_FORMAT_TOML, (const uint8_t*)toml, strlen(toml), OLIB_FORMAT_BINARY, &binary, &size));
  ASSERT_NE(binary, nullptr);

  olib_object_t* parsed = olib_format_read(OLIB_FORMAT_BINARY, binary, size);
  verify_test_object(parsed);

  olib_free(toml);
  olib_free(binary);
  olib_object_free(original);
  olib_object_free(parsed);
}

TEST(Conversion, BinaryToJson) {
  olib_object_t* original = create_test_object();

  uint8_t* binary = nullptr;
  size_t size = 0;
  EXPECT_TRUE(olib_format_write(OLIB_FORMAT_BINARY, original, &binary, &size));

  uint8_t* json_data = nullptr;
  size_t json_size = 0;
  EXPECT_TRUE(olib_convert(OLIB_FORMAT_BINARY, binary, size, OLIB_FORMAT_JSON_TEXT, &json_data, &json_size));
  ASSERT_NE(json_data, nullptr);

  olib_object_t* parsed = olib_format_read_string(OLIB_FORMAT_JSON_TEXT, (const char*)json_data);
  verify_test_object(parsed);

  olib_free(binary);
  olib_free(json_data);
  olib_object_free(original);
  olib_object_free(parsed);
}

TEST(Conversion, JsonToTxt) {
  olib_object_t* original = create_test_object();

  char* json = nullptr;
  EXPECT_TRUE(olib_format_write_string(OLIB_FORMAT_JSON_TEXT, original, &json));

  char* txt = nullptr;
  EXPECT_TRUE(olib_convert_string(OLIB_FORMAT_JSON_TEXT, json, OLIB_FORMAT_TXT, &txt));
  ASSERT_NE(txt, nullptr);

  olib_object_t* parsed = olib_format_read_string(OLIB_FORMAT_TXT, txt);
  verify_test_object(parsed);

  olib_free(json);
  olib_free(txt);
  olib_object_free(original);
  olib_object_free(parsed);
}
