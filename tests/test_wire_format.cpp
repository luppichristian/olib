#include "test_utils.h"

#include <array>
#include <cstring>

namespace {
// A list containing integer -2, string "A", and false. Integers and lengths
// in both binary formats use little-endian encoding.
constexpr std::array<uint8_t, 22> fixture = {
    0x06, 0x03, 0x00, 0x00, 0x00,
    0x01, 0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0x04, 0x01, 0x00, 0x00, 0x00, 'A',
    0x05, 0x00};
}

TEST(BinaryWireFormat, DecodesKnownFixtureAndRejectsEveryTruncation) {
  for (auto format : {OLIB_FORMAT_BINARY, OLIB_FORMAT_JSON_BINARY}) {
    SCOPED_TRACE(static_cast<int>(format));
    for (size_t length = 0; length < fixture.size(); ++length) {
      SCOPED_TRACE(length);
      olib_object_t* truncated = olib_format_read(format, fixture.data(), length);
      EXPECT_EQ(truncated, nullptr);
      olib_object_free(truncated);
    }
    olib_object_t* root = olib_format_read(format, fixture.data(), fixture.size());
    ASSERT_NE(root, nullptr);
    ASSERT_EQ(olib_object_get_type(root), OLIB_OBJECT_TYPE_LIST);
    ASSERT_EQ(olib_object_list_size(root), 3u);
    auto* number = olib_object_list_get(root, 0);
    auto* string = olib_object_list_get(root, 1);
    auto* boolean = olib_object_list_get(root, 2);
    ASSERT_NE(number, nullptr);
    ASSERT_NE(string, nullptr);
    ASSERT_NE(boolean, nullptr);
    EXPECT_EQ(olib_object_get_type(number), OLIB_OBJECT_TYPE_INT);
    EXPECT_EQ(olib_object_get_int(number), -2);
    EXPECT_EQ(olib_object_get_type(string), OLIB_OBJECT_TYPE_STRING);
    EXPECT_STREQ(olib_object_get_string(string), "A");
    EXPECT_EQ(olib_object_get_type(boolean), OLIB_OBJECT_TYPE_BOOL);
    EXPECT_FALSE(olib_object_get_bool(boolean));
    olib_object_free(root);
  }
}

TEST(BinaryWireFormat, EncodesKnownFixture) {
  for (auto format : {OLIB_FORMAT_BINARY, OLIB_FORMAT_JSON_BINARY}) {
    SCOPED_TRACE(static_cast<int>(format));
    auto* root = olib_object_new(OLIB_OBJECT_TYPE_LIST);
    auto* number = olib_object_new(OLIB_OBJECT_TYPE_INT);
    auto* string = olib_object_new(OLIB_OBJECT_TYPE_STRING);
    auto* boolean = olib_object_new(OLIB_OBJECT_TYPE_BOOL);
    ASSERT_NE(root, nullptr);
    ASSERT_NE(number, nullptr);
    ASSERT_NE(string, nullptr);
    ASSERT_NE(boolean, nullptr);
    ASSERT_TRUE(olib_object_set_int(number, -2));
    ASSERT_TRUE(olib_object_set_string(string, "A"));
    ASSERT_TRUE(olib_object_set_bool(boolean, false));
    ASSERT_TRUE(olib_object_list_push(root, number));
    ASSERT_TRUE(olib_object_list_push(root, string));
    ASSERT_TRUE(olib_object_list_push(root, boolean));
    uint8_t* bytes = nullptr;
    size_t size = 0;
    ASSERT_TRUE(olib_format_write(format, root, &bytes, &size));
    ASSERT_NE(bytes, nullptr);
    ASSERT_EQ(size, fixture.size());
    EXPECT_EQ(std::memcmp(bytes, fixture.data(), size), 0);
    olib_free(bytes);
    olib_object_free(root);
  }
}

TEST(BinaryWireFormat, RejectsInvalidBooleanPayloads) {
  for (auto format : {OLIB_FORMAT_BINARY, OLIB_FORMAT_JSON_BINARY}) {
    SCOPED_TRACE(static_cast<int>(format));
    for (uint8_t invalid : {uint8_t{2}, uint8_t{255}}) {
      const uint8_t input[] = {0x05, invalid};
      auto* parsed = olib_format_read(format, input, sizeof(input));
      EXPECT_EQ(parsed, nullptr);
      olib_object_free(parsed);
    }
  }
}
