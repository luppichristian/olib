#include <gtest/gtest.h>
#include <olib.h>

#include <cstdlib>
#include <string>
#include <unordered_set>

namespace {

// Track library allocations independently of the C++ test framework.  Failure
// injection is enabled only after each test fixture has been constructed.
std::unordered_set<void*> live_allocations;
size_t allocation_attempt = 0;
size_t fail_allocation = 0;
bool allocation_failed = false;
size_t invalid_frees = 0;

bool should_fail() {
  ++allocation_attempt;
  if (fail_allocation != 0 && allocation_attempt == fail_allocation) {
    allocation_failed = true;
    return true;
  }
  return false;
}

void* tracked_malloc(size_t size) {
  if (should_fail()) return nullptr;
  void* p = std::malloc(size);
  if (p) live_allocations.insert(p);
  return p;
}

void* tracked_calloc(size_t count, size_t size) {
  if (should_fail()) return nullptr;
  void* p = std::calloc(count, size);
  if (p) live_allocations.insert(p);
  return p;
}

void tracked_free(void* p) {
  if (!p) return;
  if (live_allocations.erase(p) != 1) {
    ++invalid_frees;
    return;
  }
  std::free(p);
}

void* tracked_realloc(void* p, size_t size) {
  if (should_fail()) return nullptr;
  if (p && live_allocations.count(p) != 1) {
    ++invalid_frees;
    return nullptr;
  }
  // The library never requests a zero-size reallocation in these tests.
  if (size == 0) return nullptr;
  if (p) live_allocations.erase(p);
  void* result = std::realloc(p, size);
  if (result) live_allocations.insert(result);
  else if (p) live_allocations.insert(p);
  return result;
}

class AllocationScope {
 public:
  AllocationScope() {
    EXPECT_TRUE(live_allocations.empty());
    invalid_frees = 0;
    fail_allocation = 0;
    allocation_attempt = 0;
    allocation_failed = false;
    olib_set_memory_fns(tracked_malloc, tracked_free, tracked_calloc, tracked_realloc);
  }
  ~AllocationScope() {
    EXPECT_EQ(invalid_frees, 0u);
    EXPECT_TRUE(live_allocations.empty()) << live_allocations.size() << " allocations remain";
    olib_set_memory_fns(std::malloc, std::free, std::calloc, std::realloc);
  }
};

void inject_at(size_t attempt) {
  allocation_attempt = 0;
  fail_allocation = attempt;
  allocation_failed = false;
}

void disable_failure() { fail_allocation = 0; }

const char* nested_json = R"({"name":"example","values":[1,2,{"inner":"longer value"}],"enabled":true})";

void expect_nested_object(olib_object_t* object) {
  ASSERT_NE(object, nullptr);
  ASSERT_EQ(olib_object_get_type(object), OLIB_OBJECT_TYPE_STRUCT);
  EXPECT_EQ(olib_object_struct_size(object), 3u);
  EXPECT_STREQ(olib_object_get_string(olib_object_struct_get(object, "name")), "example");
  auto* values = olib_object_struct_get(object, "values");
  ASSERT_NE(values, nullptr);
  ASSERT_EQ(olib_object_list_size(values), 3u);
  auto* inner = olib_object_list_get(values, 2);
  ASSERT_NE(inner, nullptr);
  EXPECT_STREQ(olib_object_get_string(olib_object_struct_get(inner, "inner")), "longer value");
  EXPECT_TRUE(olib_object_get_bool(olib_object_struct_get(object, "enabled")));
}

TEST(AllocationFailure, NestedDuplicatePreservesOriginalAndReleasesPartialCopies) {
  AllocationScope scope;
  auto* source = olib_format_read_string(OLIB_FORMAT_JSON_TEXT, nested_json);
  ASSERT_NE(source, nullptr);
  bool completed = false;
  for (size_t n = 1; n < 256; ++n) {
    SCOPED_TRACE(n);
    const size_t baseline = live_allocations.size();
    inject_at(n);
    auto* copy = olib_object_dupe(source);
    const bool failed = allocation_failed;
    disable_failure();
    if (failed) EXPECT_EQ(copy, nullptr);
    else {
      ASSERT_NE(copy, nullptr);
      expect_nested_object(copy);
      completed = true;
    }
    expect_nested_object(source);
    olib_object_free(copy);
    EXPECT_EQ(live_allocations.size(), baseline);
    if (completed) break;
  }
  EXPECT_TRUE(completed);
  olib_object_free(source);
}

TEST(AllocationFailure, JsonReadCleansPartialTreeAndSerializerCanBeReused) {
  AllocationScope scope;
  bool completed = false;
  for (size_t n = 1; n < 256; ++n) {
    SCOPED_TRACE(n);
    auto* serializer = olib_serializer_new_json_text();
    ASSERT_NE(serializer, nullptr);
    inject_at(n);
    auto* parsed = olib_serializer_read_string(serializer, nested_json);
    const bool failed = allocation_failed;
    disable_failure();
    if (failed) EXPECT_EQ(parsed, nullptr);
    else {
      ASSERT_NE(parsed, nullptr);
      expect_nested_object(parsed);
      completed = true;
    }
    olib_object_free(parsed);
    auto* retry = olib_serializer_read_string(serializer, nested_json);
    expect_nested_object(retry);
    olib_object_free(retry);
    olib_serializer_free(serializer);
    EXPECT_TRUE(live_allocations.empty());
    if (completed) break;
  }
  EXPECT_TRUE(completed);
}

TEST(AllocationFailure, TomlQuotedRootKeyNeverBecomesScalarAfterFailure) {
  AllocationScope scope;
  const char* input = "\"quoted key\" = \"value\"\n";
  bool completed = false;
  for (size_t n = 1; n < 128; ++n) {
    SCOPED_TRACE(n);
    auto* serializer = olib_serializer_new_toml();
    ASSERT_NE(serializer, nullptr);
    inject_at(n);
    auto* parsed = olib_serializer_read_string(serializer, input);
    const bool failed = allocation_failed;
    disable_failure();
    if (parsed) {
      EXPECT_EQ(olib_object_get_type(parsed), OLIB_OBJECT_TYPE_STRUCT);
      EXPECT_EQ(olib_object_struct_size(parsed), 1u);
      auto* value = olib_object_struct_get(parsed, "quoted key");
      EXPECT_NE(value, nullptr);
      if (value) {
        EXPECT_EQ(olib_object_get_type(value), OLIB_OBJECT_TYPE_STRING);
        EXPECT_STREQ(olib_object_get_string(value), "value");
      }
    }
    if (!failed) {
      EXPECT_NE(parsed, nullptr);
      completed = true;
    }
    olib_object_free(parsed);
    auto* retry = olib_serializer_read_string(serializer, input);
    ASSERT_NE(retry, nullptr);
    EXPECT_EQ(olib_object_get_type(retry), OLIB_OBJECT_TYPE_STRUCT);
    EXPECT_STREQ(olib_object_get_string(olib_object_struct_get(retry, "quoted key")), "value");
    olib_object_free(retry);
    olib_serializer_free(serializer);
    EXPECT_TRUE(live_allocations.empty());
    if (completed) break;
  }
  EXPECT_TRUE(completed);
}

TEST(AllocationFailure, SerializerFactoriesReleasePartialConstruction) {
  AllocationScope scope;
  for (int format = 0; format < OLIB_FORMAT_MAX; ++format) {
    SCOPED_TRACE(format);
    bool completed = false;
    for (size_t n = 1; n < 64; ++n) {
      SCOPED_TRACE(n);
      inject_at(n);
      auto* serializer = olib_format_serializer(static_cast<olib_format_t>(format));
      const bool failed = allocation_failed;
      disable_failure();
      if (failed) EXPECT_EQ(serializer, nullptr);
      else {
        EXPECT_NE(serializer, nullptr);
        completed = true;
      }
      olib_serializer_free(serializer);
      EXPECT_TRUE(live_allocations.empty());
      if (completed) break;
    }
    EXPECT_TRUE(completed);
  }
}

TEST(AllocationFailure, RepeatedTextReadsReleaseTemporaryStorage) {
  AllocationScope scope;
  auto* source = olib_format_read_string(OLIB_FORMAT_JSON_TEXT, R"({"name":"example"})");
  ASSERT_NE(source, nullptr);
  const olib_format_t formats[] = {OLIB_FORMAT_JSON_TEXT, OLIB_FORMAT_YAML, OLIB_FORMAT_XML,
                                   OLIB_FORMAT_TOML, OLIB_FORMAT_TXT};
  for (auto format : formats) {
    SCOPED_TRACE(static_cast<int>(format));
    const size_t baseline = live_allocations.size();
    auto* serializer = olib_format_serializer(format);
    ASSERT_NE(serializer, nullptr);
    char* encoded = nullptr;
    ASSERT_TRUE(olib_serializer_write_string(serializer, source, &encoded));
    ASSERT_NE(encoded, nullptr);
    for (int i = 0; i < 3; ++i) {
      SCOPED_TRACE(i);
      auto* value = olib_serializer_read_string(serializer, encoded);
      ASSERT_NE(value, nullptr) << "serialized input: " << encoded;
      olib_object_free(value);
    }
    olib_free(encoded);
    olib_serializer_free(serializer);
    EXPECT_EQ(live_allocations.size(), baseline);
  }
  olib_object_free(source);
}

TEST(AllocationFailure, BinaryWriteClearsOutputAndSerializerCanBeReused) {
  AllocationScope scope;
  auto* source = olib_format_read_string(OLIB_FORMAT_JSON_TEXT, nested_json);
  ASSERT_NE(source, nullptr);
  // Force multiple buffer growths rather than exercising only initial allocation.
  auto* large = olib_object_new(OLIB_OBJECT_TYPE_STRING);
  ASSERT_NE(large, nullptr);
  std::string text(2048, 'x');
  ASSERT_TRUE(olib_object_set_string(large, text.c_str()));
  ASSERT_TRUE(olib_object_struct_add(source, "large", large));
  bool completed = false;
  for (size_t n = 1; n < 256; ++n) {
    SCOPED_TRACE(n);
    auto* serializer = olib_serializer_new_binary();
    ASSERT_NE(serializer, nullptr);
    uint8_t* output = reinterpret_cast<uint8_t*>(1);
    size_t output_size = 99;
    inject_at(n);
    const bool written = olib_serializer_write(serializer, source, &output, &output_size);
    const bool failed = allocation_failed;
    disable_failure();
    if (failed) {
      EXPECT_FALSE(written);
      EXPECT_EQ(output, nullptr);
      EXPECT_EQ(output_size, 0u);
    } else {
      EXPECT_TRUE(written);
      EXPECT_NE(output, nullptr);
      EXPECT_GT(output_size, 0u);
      completed = true;
    }
    if (output != reinterpret_cast<uint8_t*>(1)) olib_free(output);
    output = nullptr;
    output_size = 0;
    ASSERT_TRUE(olib_serializer_write(serializer, source, &output, &output_size));
    ASSERT_NE(output, nullptr);
    auto* retry = olib_serializer_read(serializer, output, output_size);
    ASSERT_NE(retry, nullptr);
    EXPECT_STREQ(olib_object_get_string(olib_object_struct_get(retry, "large")), text.c_str());
    olib_object_free(retry);
    olib_free(output);
    olib_serializer_free(serializer);
    if (completed) break;
  }
  EXPECT_TRUE(completed);
  olib_object_free(source);
}

TEST(AllocationFailure, ContainerGrowthLeavesOwnershipWithCallerOnFailure) {
  AllocationScope scope;
  for (bool is_struct : {false, true}) {
    bool completed = false;
    for (size_t n = 1; n < 16; ++n) {
      SCOPED_TRACE(n);
      auto* container = olib_object_new(is_struct ? OLIB_OBJECT_TYPE_STRUCT : OLIB_OBJECT_TYPE_LIST);
      auto* child = olib_object_new(OLIB_OBJECT_TYPE_INT);
      ASSERT_NE(container, nullptr);
      ASSERT_NE(child, nullptr);
      ASSERT_TRUE(olib_object_set_int(child, 42));
      inject_at(n);
      const bool inserted = is_struct ? olib_object_struct_add(container, "key", child)
                                      : olib_object_list_push(container, child);
      const bool failed = allocation_failed;
      disable_failure();
      if (failed) {
        EXPECT_FALSE(inserted);
        EXPECT_EQ(is_struct ? olib_object_struct_size(container) : olib_object_list_size(container), 0u);
        EXPECT_EQ(olib_object_get_int(child), 42);
        olib_object_free(child);
      } else {
        EXPECT_TRUE(inserted);
        completed = true;
      }
      olib_object_free(container);
      EXPECT_TRUE(live_allocations.empty());
      if (completed) break;
    }
    EXPECT_TRUE(completed);
  }
}

struct CallbackState {
  int step = 0;
  int fail_at = 0;
  bool failed = false;
  int peeks = 0;
};

bool callback_step(CallbackState* state) {
  if (++state->step == state->fail_at) {
    state->failed = true;
    return false;
  }
  return true;
}

bool cb_init_write(void* p) { return callback_step(static_cast<CallbackState*>(p)); }
bool cb_write_list_begin(void* p, size_t size) {
  return size == 1 && callback_step(static_cast<CallbackState*>(p));
}
bool cb_write_int(void* p, int64_t value) {
  return value == 42 && callback_step(static_cast<CallbackState*>(p));
}
bool cb_write_list_end(void* p) { return callback_step(static_cast<CallbackState*>(p)); }
bool cb_finish_write(void* p, uint8_t** data, size_t* size) {
  if (!callback_step(static_cast<CallbackState*>(p))) return false;
  *data = static_cast<uint8_t*>(olib_malloc(1));
  if (!*data) return false;
  (*data)[0] = 42;
  *size = 1;
  return true;
}

TEST(CallbackFailure, WriteFailuresClearOutputAndSerializerIsReusable) {
  auto* list = olib_object_new(OLIB_OBJECT_TYPE_LIST);
  auto* item = olib_object_new(OLIB_OBJECT_TYPE_INT);
  ASSERT_NE(list, nullptr);
  ASSERT_NE(item, nullptr);
  ASSERT_TRUE(olib_object_set_int(item, 42));
  ASSERT_TRUE(olib_object_list_push(list, item));
  CallbackState state;
  olib_serializer_config_t config = {};
  config.user_data = &state;
  config.init_write = cb_init_write;
  config.write_list_begin = cb_write_list_begin;
  config.write_int = cb_write_int;
  config.write_list_end = cb_write_list_end;
  config.finish_write = cb_finish_write;
  auto* serializer = olib_serializer_new(&config);
  ASSERT_NE(serializer, nullptr);
  for (int n = 1; n <= 5; ++n) {
    SCOPED_TRACE(n);
    state = {};
    state.fail_at = n;
    uint8_t* output = reinterpret_cast<uint8_t*>(1);
    size_t size = 99;
    EXPECT_FALSE(olib_serializer_write(serializer, list, &output, &size));
    EXPECT_TRUE(state.failed);
    EXPECT_EQ(output, nullptr);
    EXPECT_EQ(size, 0u);
    state = {};
    ASSERT_TRUE(olib_serializer_write(serializer, list, &output, &size));
    ASSERT_NE(output, nullptr);
    ASSERT_EQ(size, 1u);
    EXPECT_EQ(output[0], 42);
    olib_free(output);
  }
  olib_serializer_free(serializer);
  olib_object_free(list);
}

bool cb_init_read(void* p, const uint8_t*, size_t) { return callback_step(static_cast<CallbackState*>(p)); }
olib_object_type_t cb_read_peek(void* p) {
  auto* state = static_cast<CallbackState*>(p);
  if (!callback_step(state)) return OLIB_OBJECT_TYPE_MAX;
  return state->peeks++ == 0 ? OLIB_OBJECT_TYPE_LIST : OLIB_OBJECT_TYPE_INT;
}
bool cb_read_list_begin(void* p, size_t* size) {
  if (!callback_step(static_cast<CallbackState*>(p))) return false;
  *size = 1;
  return true;
}
bool cb_read_int(void* p, int64_t* value) {
  if (!callback_step(static_cast<CallbackState*>(p))) return false;
  *value = 42;
  return true;
}
bool cb_read_list_end(void* p) { return callback_step(static_cast<CallbackState*>(p)); }
bool cb_finish_read(void* p) { return callback_step(static_cast<CallbackState*>(p)); }

TEST(CallbackFailure, ReadFailuresRejectPartialResultAndSerializerIsReusable) {
  AllocationScope scope;
  CallbackState state;
  olib_serializer_config_t config = {};
  config.user_data = &state;
  config.init_read = cb_init_read;
  config.read_peek = cb_read_peek;
  config.read_list_begin = cb_read_list_begin;
  config.read_int = cb_read_int;
  config.read_list_end = cb_read_list_end;
  config.finish_read = cb_finish_read;
  auto* serializer = olib_serializer_new(&config);
  ASSERT_NE(serializer, nullptr);
  const uint8_t input[] = {42};
  for (int n = 1; n <= 7; ++n) {
    SCOPED_TRACE(n);
    const size_t baseline = live_allocations.size();
    state = {};
    state.fail_at = n;
    auto* value = olib_serializer_read(serializer, input, sizeof(input));
    EXPECT_TRUE(state.failed);
    EXPECT_EQ(value, nullptr);
    olib_object_free(value);
    EXPECT_EQ(live_allocations.size(), baseline);
    state = {};
    value = olib_serializer_read(serializer, input, sizeof(input));
    ASSERT_NE(value, nullptr);
    ASSERT_EQ(olib_object_get_type(value), OLIB_OBJECT_TYPE_LIST);
    ASSERT_EQ(olib_object_list_size(value), 1u);
    EXPECT_EQ(olib_object_get_int(olib_object_list_get(value, 0)), 42);
    olib_object_free(value);
    EXPECT_EQ(live_allocations.size(), baseline);
  }
  olib_serializer_free(serializer);
}

}  // namespace
