#include "test_utils.h"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <string>

#if !defined(_WIN32)
#include <unistd.h>
#endif

namespace {
struct File {
  FILE* handle = std::tmpfile();
  ~File() { if (handle) std::fclose(handle); }
};

struct TempPath {
  std::filesystem::path path = std::filesystem::temp_directory_path() /
      ("olib-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  ~TempPath() { std::error_code ec; std::filesystem::remove(path, ec); }
};

void expect_int(olib_object_t* object, int64_t expected) {
  ASSERT_NE(object, nullptr);
  EXPECT_EQ(olib_object_get_type(object), OLIB_OBJECT_TYPE_INT);
  EXPECT_EQ(olib_object_get_int(object), expected);
}
}  // namespace

TEST(FileIo, FormatStreamsReadFromCurrentPositionForTextAndBinary) {
  for (auto format : {OLIB_FORMAT_JSON_TEXT, OLIB_FORMAT_BINARY}) {
    SCOPED_TRACE(static_cast<int>(format));
    File file;
    ASSERT_NE(file.handle, nullptr);
    ASSERT_EQ(std::fwrite("prefix", 1, 6, file.handle), 6u);
    olib_object_t* original = olib_object_new(OLIB_OBJECT_TYPE_INT);
    ASSERT_NE(original, nullptr);
    ASSERT_TRUE(olib_object_set_int(original, -42));
    ASSERT_TRUE(olib_format_write_file(format, original, file.handle));
    ASSERT_EQ(std::fflush(file.handle), 0);
    ASSERT_EQ(std::fseek(file.handle, 6, SEEK_SET), 0);
    olib_object_t* parsed = olib_format_read_file(format, file.handle);
    expect_int(parsed, -42);
    olib_object_free(parsed);
    olib_object_free(original);
  }
}

TEST(FileIo, SerializerPathsAndFormatPathsRoundTrip) {
  for (auto format : {OLIB_FORMAT_JSON_TEXT, OLIB_FORMAT_BINARY}) {
    SCOPED_TRACE(static_cast<int>(format));
    TempPath file;
    const std::string path = file.path.string();
    olib_serializer_t* serializer = olib_format_serializer(format);
    olib_object_t* original = olib_object_new(OLIB_OBJECT_TYPE_INT);
    ASSERT_NE(serializer, nullptr);
    ASSERT_NE(original, nullptr);
    ASSERT_TRUE(olib_object_set_int(original, -912));
    ASSERT_TRUE(olib_serializer_write_file_path(serializer, original, path.c_str()));
    olib_object_t* parsed = olib_format_read_file_path(format, path.c_str());
    expect_int(parsed, -912);
    olib_object_free(parsed);
    ASSERT_TRUE(olib_format_write_file_path(format, original, path.c_str()));
    parsed = olib_serializer_read_file_path(serializer, path.c_str());
    expect_int(parsed, -912);
    olib_object_free(parsed);
    olib_object_free(original);
    olib_serializer_free(serializer);
  }
}

TEST(FileIo, ConversionSupportsStreamsAndPaths) {
  File source;
  File destination;
  ASSERT_NE(source.handle, nullptr);
  ASSERT_NE(destination.handle, nullptr);
  ASSERT_GE(std::fputs("-25", source.handle), 0);
  ASSERT_EQ(std::fflush(source.handle), 0);
  std::rewind(source.handle);
  ASSERT_TRUE(olib_convert_file(OLIB_FORMAT_JSON_TEXT, source.handle,
                                OLIB_FORMAT_BINARY, destination.handle));
  ASSERT_EQ(std::fflush(destination.handle), 0);
  std::rewind(destination.handle);
  olib_object_t* parsed = olib_format_read_file(OLIB_FORMAT_BINARY, destination.handle);
  expect_int(parsed, -25);
  olib_object_free(parsed);

  TempPath input;
  TempPath output;
  olib_object_t* original = olib_object_new(OLIB_OBJECT_TYPE_INT);
  ASSERT_NE(original, nullptr);
  ASSERT_TRUE(olib_object_set_int(original, -25));
  ASSERT_TRUE(olib_format_write_file_path(OLIB_FORMAT_JSON_TEXT, original, input.path.string().c_str()));
  ASSERT_TRUE(olib_convert_file_path(OLIB_FORMAT_JSON_TEXT, input.path.string().c_str(),
                                     OLIB_FORMAT_BINARY, output.path.string().c_str()));
  parsed = olib_format_read_file_path(OLIB_FORMAT_BINARY, output.path.string().c_str());
  expect_int(parsed, -25);
  olib_object_free(parsed);
  olib_object_free(original);
}

TEST(FileIo, EmptyAndMissingFilesFailAndDoNotProduceConversionOutput) {
  File empty;
  File destination;
  ASSERT_NE(empty.handle, nullptr);
  ASSERT_NE(destination.handle, nullptr);
  EXPECT_EQ(olib_format_read_file(OLIB_FORMAT_JSON_TEXT, empty.handle), nullptr);
  EXPECT_FALSE(olib_convert_file(OLIB_FORMAT_JSON_TEXT, empty.handle,
                                 OLIB_FORMAT_BINARY, destination.handle));
  EXPECT_EQ(std::ftell(destination.handle), 0);
  TempPath missing;
  const auto bad_path = (missing.path / "missing").string();
  EXPECT_EQ(olib_format_read_file_path(OLIB_FORMAT_JSON_TEXT, bad_path.c_str()), nullptr);
  olib_object_t* value = olib_object_new(OLIB_OBJECT_TYPE_INT);
  ASSERT_NE(value, nullptr);
  EXPECT_FALSE(olib_format_write_file_path(OLIB_FORMAT_JSON_TEXT, value, bad_path.c_str()));
  EXPECT_FALSE(olib_convert_file_path(OLIB_FORMAT_JSON_TEXT, bad_path.c_str(),
                                     OLIB_FORMAT_BINARY, missing.path.string().c_str()));
  EXPECT_FALSE(std::filesystem::exists(missing.path));
  olib_object_free(value);
}

TEST(FileIo, RejectsStreamPositionBeyondEnd) {
  File file;
  ASSERT_NE(file.handle, nullptr);
  ASSERT_EQ(std::fwrite("-42", 1, 3, file.handle), 3u);
  ASSERT_EQ(std::fflush(file.handle), 0);
  ASSERT_EQ(std::fseek(file.handle, 20, SEEK_SET), 0);
  auto* parsed = olib_format_read_file(OLIB_FORMAT_JSON_TEXT, file.handle);
  EXPECT_EQ(parsed, nullptr);
  olib_object_free(parsed);
}

#if !defined(_WIN32)
TEST(FileIo, RejectsNonSeekableInputStream) {
  int fds[2];
  ASSERT_EQ(pipe(fds), 0);
  ASSERT_EQ(write(fds[1], "-42", 3), 3);
  ASSERT_EQ(close(fds[1]), 0);
  FILE* input = fdopen(fds[0], "rb");
  ASSERT_NE(input, nullptr);
  olib_object_t* parsed = olib_format_read_file(OLIB_FORMAT_JSON_TEXT, input);
  EXPECT_EQ(parsed, nullptr);
  olib_object_free(parsed);
  EXPECT_EQ(std::fclose(input), 0);
}
#endif

#if defined(__linux__)
TEST(FileIo, PathWriteReportsDeferredCloseFailure) {
  // /dev/full accepts buffered writes but reports ENOSPC when they are flushed.
  auto* value = olib_object_new(OLIB_OBJECT_TYPE_INT);
  ASSERT_NE(value, nullptr);
  ASSERT_TRUE(olib_object_set_int(value, -42));
  EXPECT_FALSE(olib_format_write_file_path(OLIB_FORMAT_JSON_TEXT, value, "/dev/full"));
  olib_object_free(value);
}
#endif
