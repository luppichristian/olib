#include <gtest/gtest.h>
#include <olib.h>
#include <cstdlib>
#include <cstring>
#include <vector>

TEST(ObjectContract, RejectsInvalidTypes)
{
    EXPECT_EQ(olib_object_new(OLIB_OBJECT_TYPE_MAX), nullptr);
    EXPECT_EQ(olib_object_new(static_cast<olib_object_type_t>(-1)), nullptr);
}

TEST(ObjectContract, ScalarGettersConvertCompatibleValues)
{
    olib_object_t* string = olib_object_new(OLIB_OBJECT_TYPE_STRING);
    ASSERT_NE(string, nullptr);
    ASSERT_TRUE(olib_object_set_string(string, "123"));

    EXPECT_EQ(olib_object_get_int(string), 123);
    EXPECT_EQ(olib_object_get_uint(string), 123u);
    EXPECT_DOUBLE_EQ(olib_object_get_float(string), 123.0);
    EXPECT_FALSE(olib_object_get_bool(string));

    olib_object_t* integer = olib_object_new(OLIB_OBJECT_TYPE_INT);
    ASSERT_NE(integer, nullptr);
    ASSERT_TRUE(olib_object_set_int(integer, 1));
    EXPECT_EQ(olib_object_get_string(integer), nullptr);
    EXPECT_EQ(olib_object_get_uint(integer), 1u);
    EXPECT_DOUBLE_EQ(olib_object_get_float(integer), 1.0);
    EXPECT_TRUE(olib_object_get_bool(integer));

    olib_object_t* list = olib_object_new(OLIB_OBJECT_TYPE_LIST);
    ASSERT_NE(list, nullptr);
    EXPECT_EQ(olib_object_get_int(list), 0);
    EXPECT_EQ(olib_object_get_uint(list), 0u);
    EXPECT_DOUBLE_EQ(olib_object_get_float(list), 0.0);
    EXPECT_EQ(olib_object_get_string(list), nullptr);
    EXPECT_FALSE(olib_object_get_bool(list));

    olib_object_free(string);
    olib_object_free(integer);
    olib_object_free(list);
}

static void* failing_malloc(size_t)
{
    return nullptr;
}

static size_t g_allocation_attempt = 0;
static size_t g_fail_on_allocation = 0;
static size_t g_live_allocations = 0;

static void* tracked_malloc(size_t size)
{
    if (++g_allocation_attempt == g_fail_on_allocation) {
        return nullptr;
    }
    void* ptr = std::malloc(size);
    if (ptr) {
        ++g_live_allocations;
    }
    return ptr;
}

static void* tracked_calloc(size_t count, size_t size)
{
    if (++g_allocation_attempt == g_fail_on_allocation) {
        return nullptr;
    }
    void* ptr = std::calloc(count, size);
    if (ptr) {
        ++g_live_allocations;
    }
    return ptr;
}

static void tracked_free(void* ptr)
{
    if (ptr) {
        --g_live_allocations;
    }
    std::free(ptr);
}

static void* tracked_realloc(void* ptr, size_t size)
{
    if (++g_allocation_attempt == g_fail_on_allocation) {
        return nullptr;
    }
    void* replacement = std::realloc(ptr, size);
    if (!ptr && replacement) {
        ++g_live_allocations;
    }
    return replacement;
}

TEST(ObjectContract, DuplicateStructCleansUpAfterAllocationFailure)
{
    olib_object_t* original = olib_object_new(OLIB_OBJECT_TYPE_STRUCT);
    olib_object_t* value = olib_object_new(OLIB_OBJECT_TYPE_INT);
    ASSERT_NE(original, nullptr);
    ASSERT_NE(value, nullptr);
    ASSERT_TRUE(olib_object_set_int(value, 9));
    ASSERT_TRUE(olib_object_struct_add(original, "value", value));

    g_allocation_attempt = 0;
    g_fail_on_allocation = 4;
    g_live_allocations = 0;
    olib_set_memory_fns(tracked_malloc, tracked_free, tracked_calloc, tracked_realloc);
    EXPECT_EQ(olib_object_dupe(original), nullptr);
    olib_set_memory_fns(std::malloc, std::free, std::calloc, std::realloc);

    EXPECT_EQ(g_live_allocations, 0u);
    olib_object_free(original);
}

TEST(ObjectContract, StringSetterPreservesValueOnAllocationFailure)
{
    olib_object_t* string = olib_object_new(OLIB_OBJECT_TYPE_STRING);
    ASSERT_NE(string, nullptr);
    ASSERT_TRUE(olib_object_set_string(string, "original"));

    olib_set_memory_fns(failing_malloc, std::free, std::calloc, std::realloc);
    EXPECT_FALSE(olib_object_set_string(string, "replacement"));
    olib_set_memory_fns(std::malloc, std::free, std::calloc, std::realloc);

    EXPECT_STREQ(olib_object_get_string(string), "original");
    olib_object_free(string);
}

TEST(ObjectContract, StringSetterAcceptsItsCurrentValue)
{
    olib_object_t* string = olib_object_new(OLIB_OBJECT_TYPE_STRING);
    ASSERT_NE(string, nullptr);
    ASSERT_TRUE(olib_object_set_string(string, "stable"));

    const char* current = olib_object_get_string(string);
    ASSERT_TRUE(olib_object_set_string(string, current));
    EXPECT_STREQ(olib_object_get_string(string), "stable");

    olib_object_free(string);
}

TEST(ObjectContract, ContainerSetAcceptsItsCurrentValue)
{
    olib_object_t* list = olib_object_new(OLIB_OBJECT_TYPE_LIST);
    olib_object_t* list_value = olib_object_new(OLIB_OBJECT_TYPE_INT);
    ASSERT_NE(list, nullptr);
    ASSERT_NE(list_value, nullptr);
    ASSERT_TRUE(olib_object_set_int(list_value, 17));
    ASSERT_TRUE(olib_object_list_push(list, list_value));
    ASSERT_TRUE(olib_object_list_set(list, 0, olib_object_list_get(list, 0)));
    EXPECT_EQ(olib_object_get_int(olib_object_list_get(list, 0)), 17);

    olib_object_t* object = olib_object_new(OLIB_OBJECT_TYPE_STRUCT);
    olib_object_t* object_value = olib_object_new(OLIB_OBJECT_TYPE_INT);
    ASSERT_NE(object, nullptr);
    ASSERT_NE(object_value, nullptr);
    ASSERT_TRUE(olib_object_set_int(object_value, 23));
    ASSERT_TRUE(olib_object_struct_set(object, "value", object_value));
    ASSERT_TRUE(olib_object_struct_set(object, "value", olib_object_struct_get(object, "value")));
    EXPECT_EQ(olib_object_get_int(olib_object_struct_get(object, "value")), 23);

    olib_object_free(list);
    olib_object_free(object);
}

static bool read_init(void*, const uint8_t*, size_t)
{
    return true;
}

static olib_object_type_t read_peek_int(void*)
{
    return OLIB_OBJECT_TYPE_INT;
}

static bool read_int_value(void*, int64_t* value)
{
    *value = 7;
    return true;
}

static bool read_finish_failure(void*)
{
    return false;
}

static bool read_finish_success(void*)
{
    return true;
}

static olib_object_type_t read_peek_string(void*)
{
    return OLIB_OBJECT_TYPE_STRING;
}

static bool read_string_value(void*, const char** value)
{
    *value = "value";
    return true;
}

TEST(SerializerContract, StringAllocationFailureRejectsParsedObject)
{
    olib_serializer_config_t config = {};
    config.text_based = true;
    config.init_read = read_init;
    config.finish_read = read_finish_success;
    config.read_peek = read_peek_string;
    config.read_string = read_string_value;

    olib_serializer_t* serializer = olib_serializer_new(&config);
    ASSERT_NE(serializer, nullptr);

    olib_set_memory_fns(failing_malloc, std::free, std::calloc, std::realloc);
    olib_object_t* value = olib_serializer_read_string(serializer, "ignored");
    olib_set_memory_fns(std::malloc, std::free, std::calloc, std::realloc);

    EXPECT_EQ(value, nullptr);
    olib_object_free(value);
    olib_serializer_free(serializer);
}

static bool write_init(void*)
{
    return true;
}

static bool write_int_failure(void*, int64_t)
{
    return false;
}

TEST(SerializerContract, WriteFailureClearsOutputs)
{
    olib_serializer_config_t config = {};
    config.text_based = false;
    config.init_write = write_init;
    config.write_int = write_int_failure;

    olib_serializer_t* serializer = olib_serializer_new(&config);
    olib_object_t* value = olib_object_new(OLIB_OBJECT_TYPE_INT);
    ASSERT_NE(serializer, nullptr);
    ASSERT_NE(value, nullptr);

    uint8_t* output = reinterpret_cast<uint8_t*>(1);
    size_t output_size = 99;
    EXPECT_FALSE(olib_serializer_write(serializer, value, &output, &output_size));
    EXPECT_EQ(output, nullptr);
    EXPECT_EQ(output_size, 0u);

    olib_object_free(value);
    olib_serializer_free(serializer);
}

TEST(SerializerContract, RequiresStringOutput)
{
    olib_serializer_t* serializer = olib_serializer_new_json_text();
    olib_object_t* value = olib_object_new(OLIB_OBJECT_TYPE_INT);
    ASSERT_NE(serializer, nullptr);
    ASSERT_NE(value, nullptr);

    EXPECT_FALSE(olib_serializer_write_string(serializer, value, nullptr));

    olib_object_free(value);
    olib_serializer_free(serializer);
}

TEST(SerializerContract, FinishReadFailureRejectsResult)
{
    olib_serializer_config_t config = {};
    config.text_based = true;
    config.init_read = read_init;
    config.finish_read = read_finish_failure;
    config.read_peek = read_peek_int;
    config.read_int = read_int_value;

    olib_serializer_t* serializer = olib_serializer_new(&config);
    ASSERT_NE(serializer, nullptr);

    olib_object_t* value = olib_serializer_read_string(serializer, "7");
    EXPECT_EQ(value, nullptr);

    olib_object_free(value);
    olib_serializer_free(serializer);
}

TEST(SerializerContract, JsonRejectsTrailingData)
{
    olib_serializer_t* serializer = olib_serializer_new_json_text();
    ASSERT_NE(serializer, nullptr);

    olib_object_t* value = olib_serializer_read_string(serializer, "{\"value\": 1} trailing");
    EXPECT_EQ(value, nullptr);

    olib_object_free(value);
    olib_serializer_free(serializer);
}

TEST(SerializerContract, JsonEscapesStructKeys)
{
    const char* key = "quote\" slash\\ newline\n";
    olib_object_t* object = olib_object_new(OLIB_OBJECT_TYPE_STRUCT);
    olib_object_t* value = olib_object_new(OLIB_OBJECT_TYPE_INT);
    ASSERT_NE(object, nullptr);
    ASSERT_NE(value, nullptr);
    ASSERT_TRUE(olib_object_set_int(value, 5));
    ASSERT_TRUE(olib_object_struct_add(object, key, value));

    char* encoded = nullptr;
    ASSERT_TRUE(olib_format_write_string(OLIB_FORMAT_JSON_TEXT, object, &encoded));
    ASSERT_NE(encoded, nullptr);
    EXPECT_NE(std::strstr(encoded, "\"quote\\\" slash\\\\ newline\\n\""), nullptr);

    olib_object_t* parsed = olib_format_read_string(OLIB_FORMAT_JSON_TEXT, encoded);
    ASSERT_NE(parsed, nullptr);
    olib_object_t* parsed_value = olib_object_struct_get(parsed, key);
    ASSERT_NE(parsed_value, nullptr);
    EXPECT_EQ(olib_object_get_int(parsed_value), 5);

    olib_object_free(parsed);
    olib_free(encoded);
    olib_object_free(object);
}

TEST(SerializerContract, BinaryRejectsTrailingData)
{
    olib_serializer_t* serializer = olib_serializer_new_binary();
    olib_object_t* original = olib_object_new(OLIB_OBJECT_TYPE_INT);
    ASSERT_NE(serializer, nullptr);
    ASSERT_NE(original, nullptr);
    ASSERT_TRUE(olib_object_set_int(original, 42));

    uint8_t* encoded = nullptr;
    size_t encoded_size = 0;
    ASSERT_TRUE(olib_serializer_write(serializer, original, &encoded, &encoded_size));
    ASSERT_NE(encoded, nullptr);

    std::vector<uint8_t> with_trailing_data(encoded, encoded + encoded_size);
    with_trailing_data.push_back(0xff);
    olib_object_t* parsed = olib_serializer_read(serializer, with_trailing_data.data(), with_trailing_data.size());
    EXPECT_EQ(parsed, nullptr);

    olib_object_free(parsed);
    olib_free(encoded);
    olib_object_free(original);
    olib_serializer_free(serializer);
}

TEST(SerializerContract, BinaryFormatsRejectUnrepresentableEmptyKeys)
{
    olib_object_t* object = olib_object_new(OLIB_OBJECT_TYPE_STRUCT);
    olib_object_t* value = olib_object_new(OLIB_OBJECT_TYPE_INT);
    ASSERT_NE(object, nullptr);
    ASSERT_NE(value, nullptr);
    ASSERT_TRUE(olib_object_set_int(value, 7));
    ASSERT_TRUE(olib_object_struct_add(object, "", value));

    const olib_format_t formats[] = {OLIB_FORMAT_JSON_BINARY, OLIB_FORMAT_BINARY};
    for (olib_format_t format : formats) {
        uint8_t* encoded = nullptr;
        size_t encoded_size = 0;
        EXPECT_FALSE(olib_format_write(format, object, &encoded, &encoded_size));
        EXPECT_EQ(encoded, nullptr);
        EXPECT_EQ(encoded_size, 0u);
        olib_free(encoded);
    }

    olib_object_free(object);
}

TEST(SerializerContract, BinaryFormatsRejectEmbeddedNullStringsAndKeys)
{
    const uint8_t embedded_null_string[] = {0x04, 0x03, 0x00, 0x00, 0x00, 'a', 0x00, 'b'};
    const uint8_t embedded_null_key[] = {
        0x07,
        0x03, 0x00, 0x00, 0x00, 'a', 0x00, 'b',
        0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
    };
    const olib_format_t formats[] = {OLIB_FORMAT_JSON_BINARY, OLIB_FORMAT_BINARY};

    for (olib_format_t format : formats) {
        EXPECT_EQ(olib_format_read(format, embedded_null_string, sizeof(embedded_null_string)), nullptr);
        EXPECT_EQ(olib_format_read(format, embedded_null_key, sizeof(embedded_null_key)), nullptr);
    }
}

TEST(SerializerContract, BinaryFormatsRejectOversizedLengths)
{
    const uint8_t oversized_string[] = {0x04, 0xff, 0xff, 0xff, 0xff};
    const uint8_t oversized_key[] = {0x07, 0xff, 0xff, 0xff, 0xff};
    const olib_format_t formats[] = {OLIB_FORMAT_JSON_BINARY, OLIB_FORMAT_BINARY};

    for (olib_format_t format : formats) {
        EXPECT_EQ(olib_format_read(format, oversized_string, sizeof(oversized_string)), nullptr);
        EXPECT_EQ(olib_format_read(format, oversized_key, sizeof(oversized_key)), nullptr);
    }
}

TEST(SerializerContract, BinaryFormatsRejectMalformedStructKeys)
{
    const uint8_t malformed_struct[] = {
        0x07,
        0x04, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
    };
    const olib_format_t formats[] = {OLIB_FORMAT_JSON_BINARY, OLIB_FORMAT_BINARY};

    for (olib_format_t format : formats) {
        EXPECT_EQ(olib_format_read(format, malformed_struct, sizeof(malformed_struct)), nullptr);
    }
}

TEST(ConversionContract, TextSourceHonorsExplicitSize)
{
    const uint8_t source[] = {'4', '2', ' ', '9', '9', '\0'};
    uint8_t* converted = nullptr;
    size_t converted_size = 0;

    ASSERT_TRUE(olib_convert(
        OLIB_FORMAT_JSON_TEXT,
        source,
        2,
        OLIB_FORMAT_JSON_TEXT,
        &converted,
        &converted_size));
    ASSERT_NE(converted, nullptr);
    EXPECT_EQ(converted_size, 3u);
    EXPECT_STREQ(reinterpret_cast<const char*>(converted), "42\n");

    olib_free(converted);
}

TEST(ConversionContract, RejectsEmbeddedNullInTextSource)
{
    const uint8_t source[] = {'4', '2', '\0', ' ', '9', '9'};
    uint8_t* converted = reinterpret_cast<uint8_t*>(1);
    size_t converted_size = 99;

    EXPECT_FALSE(olib_convert(
        OLIB_FORMAT_JSON_TEXT,
        source,
        sizeof(source),
        OLIB_FORMAT_JSON_TEXT,
        &converted,
        &converted_size));
    EXPECT_EQ(converted, nullptr);
    EXPECT_EQ(converted_size, 0u);
    olib_free(converted);
}

TEST(SerializerContract, RejectsTruncatedTextContainers)
{
    const struct {
        olib_format_t format;
        const char* input;
    } cases[] = {
        {OLIB_FORMAT_JSON_TEXT, "{\"value\":"},
        {OLIB_FORMAT_XML, "<root><value type=\"int\">1</value>"},
        {OLIB_FORMAT_TOML, "value = [1,"},
        {OLIB_FORMAT_TXT, "{ value: [ 1"},
    };

    for (const auto& test_case : cases) {
        olib_object_t* parsed = olib_format_read_string(test_case.format, test_case.input);
        EXPECT_EQ(parsed, nullptr) << "format " << static_cast<int>(test_case.format);
        olib_object_free(parsed);
    }
}
