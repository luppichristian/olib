#include "test_utils.h"

// =============================================================================
// JSON Text Serializer Tests
// =============================================================================

TEST(SerializerJsonText, RoundTripInt)
{
    olib_serializer_t* ser = olib_serializer_new_json_text();
    ASSERT_NE(ser, nullptr);

    olib_object_t* original = olib_object_new(OLIB_OBJECT_TYPE_INT);
    olib_object_set_int(original, -12345);

    char* json = nullptr;
    EXPECT_TRUE(olib_serializer_write_string(ser, original, &json));
    ASSERT_NE(json, nullptr);

    olib_object_t* parsed = olib_serializer_read_string(ser, json);
    ASSERT_NE(parsed, nullptr);
    EXPECT_EQ(olib_object_get_int(parsed), -12345);

    olib_free(json);
    olib_object_free(original);
    olib_object_free(parsed);
    olib_serializer_free(ser);
}

TEST(SerializerJsonText, RoundTripComplex)
{
    olib_serializer_t* ser = olib_serializer_new_json_text();
    ASSERT_NE(ser, nullptr);

    olib_object_t* original = create_test_object();

    char* json = nullptr;
    EXPECT_TRUE(olib_serializer_write_string(ser, original, &json));
    ASSERT_NE(json, nullptr);

    olib_object_t* parsed = olib_serializer_read_string(ser, json);
    verify_test_object(parsed);

    olib_free(json);
    olib_object_free(original);
    olib_object_free(parsed);
    olib_serializer_free(ser);
}

TEST(SerializerJsonText, RoundTripMatrix)
{
    olib_serializer_t* ser = olib_serializer_new_json_text();
    ASSERT_NE(ser, nullptr);

    size_t dims[] = {2, 3};
    olib_object_t* original = olib_object_matrix_new(2, dims);
    double data[] = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0};
    olib_object_matrix_set_data(original, data, 6);

    char* json = nullptr;
    EXPECT_TRUE(olib_serializer_write_string(ser, original, &json));
    ASSERT_NE(json, nullptr);

    olib_object_t* parsed = olib_serializer_read_string(ser, json);
    ASSERT_NE(parsed, nullptr);
    EXPECT_EQ(olib_object_matrix_ndims(parsed), 2u);
    EXPECT_EQ(olib_object_matrix_dim(parsed, 0), 2u);
    EXPECT_EQ(olib_object_matrix_dim(parsed, 1), 3u);

    size_t idx[] = {1, 2};
    EXPECT_DOUBLE_EQ(olib_object_matrix_get(parsed, idx), 6.0);

    olib_free(json);
    olib_object_free(original);
    olib_object_free(parsed);
    olib_serializer_free(ser);
}

TEST(SerializerJsonText, SpecialCharacters)
{
    olib_serializer_t* ser = olib_serializer_new_json_text();

    olib_object_t* obj = olib_object_new(OLIB_OBJECT_TYPE_STRING);
    olib_object_set_string(obj, "Line1\nLine2\tTab\"Quote\"\\Backslash");

    char* json = nullptr;
    EXPECT_TRUE(olib_serializer_write_string(ser, obj, &json));

    olib_object_t* parsed = olib_serializer_read_string(ser, json);
    ASSERT_NE(parsed, nullptr);
    EXPECT_STREQ(olib_object_get_string(parsed), "Line1\nLine2\tTab\"Quote\"\\Backslash");

    olib_free(json);
    olib_object_free(obj);
    olib_object_free(parsed);
    olib_serializer_free(ser);
}

// =============================================================================
// JSON Binary Serializer Tests
// =============================================================================

TEST(SerializerJsonBinary, RoundTripComplex)
{
    olib_serializer_t* ser = olib_serializer_new_json_binary();
    ASSERT_NE(ser, nullptr);

    olib_object_t* original = create_test_object();

    uint8_t* data = nullptr;
    size_t size = 0;
    EXPECT_TRUE(olib_serializer_write(ser, original, &data, &size));
    ASSERT_NE(data, nullptr);
    EXPECT_GT(size, 0u);

    olib_object_t* parsed = olib_serializer_read(ser, data, size);
    verify_test_object(parsed);

    olib_free(data);
    olib_object_free(original);
    olib_object_free(parsed);
    olib_serializer_free(ser);
}

// =============================================================================
// YAML Serializer Tests
// =============================================================================

TEST(SerializerYaml, RoundTripComplex)
{
    olib_serializer_t* ser = olib_serializer_new_yaml();
    ASSERT_NE(ser, nullptr);

    olib_object_t* original = create_test_object();

    char* yaml = nullptr;
    EXPECT_TRUE(olib_serializer_write_string(ser, original, &yaml));
    ASSERT_NE(yaml, nullptr);

    olib_object_t* parsed = olib_serializer_read_string(ser, yaml);
    verify_test_object(parsed);

    olib_free(yaml);
    olib_object_free(original);
    olib_object_free(parsed);
    olib_serializer_free(ser);
}

TEST(SerializerYaml, RoundTripMatrix)
{
    olib_serializer_t* ser = olib_serializer_new_yaml();

    size_t dims[] = {2, 2};
    olib_object_t* original = olib_object_matrix_new(2, dims);
    olib_object_matrix_fill(original, 7.5);

    char* yaml = nullptr;
    EXPECT_TRUE(olib_serializer_write_string(ser, original, &yaml));

    olib_object_t* parsed = olib_serializer_read_string(ser, yaml);
    ASSERT_NE(parsed, nullptr);

    size_t idx[] = {1, 1};
    EXPECT_DOUBLE_EQ(olib_object_matrix_get(parsed, idx), 7.5);

    olib_free(yaml);
    olib_object_free(original);
    olib_object_free(parsed);
    olib_serializer_free(ser);
}

// =============================================================================
// XML Serializer Tests
// =============================================================================

TEST(SerializerXml, RoundTripComplex)
{
    olib_serializer_t* ser = olib_serializer_new_xml();
    ASSERT_NE(ser, nullptr);

    olib_object_t* original = create_test_object();

    char* xml = nullptr;
    EXPECT_TRUE(olib_serializer_write_string(ser, original, &xml));
    ASSERT_NE(xml, nullptr);

    olib_object_t* parsed = olib_serializer_read_string(ser, xml);
    verify_test_object(parsed);

    olib_free(xml);
    olib_object_free(original);
    olib_object_free(parsed);
    olib_serializer_free(ser);
}

TEST(SerializerXml, RoundTripMatrix)
{
    olib_serializer_t* ser = olib_serializer_new_xml();

    size_t dims[] = {3};
    olib_object_t* original = olib_object_matrix_new(1, dims);
    double data[] = {1.1, 2.2, 3.3};
    olib_object_matrix_set_data(original, data, 3);

    char* xml = nullptr;
    EXPECT_TRUE(olib_serializer_write_string(ser, original, &xml));

    olib_object_t* parsed = olib_serializer_read_string(ser, xml);
    ASSERT_NE(parsed, nullptr);

    size_t idx[] = {2};
    EXPECT_NEAR(olib_object_matrix_get(parsed, idx), 3.3, 0.0001);

    olib_free(xml);
    olib_object_free(original);
    olib_object_free(parsed);
    olib_serializer_free(ser);
}

// =============================================================================
// TOML Serializer Tests
// =============================================================================

TEST(SerializerToml, RoundTripComplex)
{
    olib_serializer_t* ser = olib_serializer_new_toml();
    ASSERT_NE(ser, nullptr);

    olib_object_t* original = create_test_object();

    char* toml = nullptr;
    EXPECT_TRUE(olib_serializer_write_string(ser, original, &toml));
    ASSERT_NE(toml, nullptr);

    olib_object_t* parsed = olib_serializer_read_string(ser, toml);
    verify_test_object(parsed);

    olib_free(toml);
    olib_object_free(original);
    olib_object_free(parsed);
    olib_serializer_free(ser);
}

// =============================================================================
// Binary Serializer Tests
// =============================================================================

TEST(SerializerBinary, RoundTripComplex)
{
    olib_serializer_t* ser = olib_serializer_new_binary();
    ASSERT_NE(ser, nullptr);

    olib_object_t* original = create_test_object();

    uint8_t* data = nullptr;
    size_t size = 0;
    EXPECT_TRUE(olib_serializer_write(ser, original, &data, &size));
    ASSERT_NE(data, nullptr);
    EXPECT_GT(size, 0u);

    olib_object_t* parsed = olib_serializer_read(ser, data, size);
    verify_test_object(parsed);

    olib_free(data);
    olib_object_free(original);
    olib_object_free(parsed);
    olib_serializer_free(ser);
}

TEST(SerializerBinary, RoundTripMatrix)
{
    olib_serializer_t* ser = olib_serializer_new_binary();

    size_t dims[] = {4, 4};
    olib_object_t* original = olib_object_matrix_new(2, dims);
    for (size_t i = 0; i < 16; i++) {
        double* data = olib_object_matrix_data(original);
        data[i] = (double)i;
    }

    uint8_t* data = nullptr;
    size_t size = 0;
    EXPECT_TRUE(olib_serializer_write(ser, original, &data, &size));

    olib_object_t* parsed = olib_serializer_read(ser, data, size);
    ASSERT_NE(parsed, nullptr);

    EXPECT_EQ(olib_object_matrix_total_size(parsed), 16u);
    double* parsed_data = olib_object_matrix_data(parsed);
    for (size_t i = 0; i < 16; i++) {
        EXPECT_DOUBLE_EQ(parsed_data[i], (double)i);
    }

    olib_free(data);
    olib_object_free(original);
    olib_object_free(parsed);
    olib_serializer_free(ser);
}

TEST(SerializerBinary, EdgeCases)
{
    olib_serializer_t* ser = olib_serializer_new_binary();

    // Test empty array
    olib_object_t* empty_arr = olib_object_new(OLIB_OBJECT_TYPE_ARRAY);
    uint8_t* data = nullptr;
    size_t size = 0;
    EXPECT_TRUE(olib_serializer_write(ser, empty_arr, &data, &size));

    olib_object_t* parsed_arr = olib_serializer_read(ser, data, size);
    ASSERT_NE(parsed_arr, nullptr);
    EXPECT_EQ(olib_object_array_size(parsed_arr), 0u);

    olib_free(data);
    olib_object_free(empty_arr);
    olib_object_free(parsed_arr);

    // Test empty struct
    olib_object_t* empty_struct = olib_object_new(OLIB_OBJECT_TYPE_STRUCT);
    data = nullptr;
    size = 0;
    EXPECT_TRUE(olib_serializer_write(ser, empty_struct, &data, &size));

    olib_object_t* parsed_struct = olib_serializer_read(ser, data, size);
    ASSERT_NE(parsed_struct, nullptr);
    EXPECT_EQ(olib_object_struct_size(parsed_struct), 0u);

    olib_free(data);
    olib_object_free(empty_struct);
    olib_object_free(parsed_struct);

    olib_serializer_free(ser);
}

// =============================================================================
// Plain Text Serializer Tests
// =============================================================================

TEST(SerializerTxt, RoundTripComplex)
{
    olib_serializer_t* ser = olib_serializer_new_txt();
    ASSERT_NE(ser, nullptr);

    olib_object_t* original = create_test_object();

    char* txt = nullptr;
    EXPECT_TRUE(olib_serializer_write_string(ser, original, &txt));
    ASSERT_NE(txt, nullptr);

    olib_object_t* parsed = olib_serializer_read_string(ser, txt);
    verify_test_object(parsed);

    olib_free(txt);
    olib_object_free(original);
    olib_object_free(parsed);
    olib_serializer_free(ser);
}

// =============================================================================
// Parameterized Test for All Formats
// =============================================================================

class SerializerFormatTest : public ::testing::TestWithParam<olib_format_t> {};

TEST_P(SerializerFormatTest, RoundTrip)
{
    olib_format_t format = GetParam();
    olib_serializer_t* ser = olib_format_serializer(format);
    ASSERT_NE(ser, nullptr);

    olib_object_t* original = create_test_object();

    bool is_text = olib_serializer_is_text_based(ser);
    
    if (is_text) {
        // Text-based serializers
        char* str = nullptr;
        ASSERT_TRUE(olib_serializer_write_string(ser, original, &str));
        ASSERT_NE(str, nullptr);

        olib_object_t* parsed = olib_serializer_read_string(ser, str);
        ASSERT_NE(parsed, nullptr);
        verify_test_object(parsed);

        olib_free(str);
        olib_object_free(parsed);
    } else {
        // Binary serializers
        uint8_t* data = nullptr;
        size_t size = 0;
        ASSERT_TRUE(olib_serializer_write(ser, original, &data, &size));
        ASSERT_NE(data, nullptr);
        ASSERT_GT(size, 0u);

        olib_object_t* parsed = olib_serializer_read(ser, data, size);
        ASSERT_NE(parsed, nullptr);
        verify_test_object(parsed);

        olib_free(data);
        olib_object_free(parsed);
    }

    olib_object_free(original);
    olib_serializer_free(ser);
}

INSTANTIATE_TEST_SUITE_P(
    AllFormats,
    SerializerFormatTest,
    ::testing::Values(
        OLIB_FORMAT_JSON_TEXT,
        OLIB_FORMAT_JSON_BINARY,
        OLIB_FORMAT_YAML,
        OLIB_FORMAT_XML,
        OLIB_FORMAT_BINARY,
        OLIB_FORMAT_TOML,
        OLIB_FORMAT_TXT
    )
);
