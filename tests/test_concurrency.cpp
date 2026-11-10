#include "test_utils.h"

#include <array>
#include <atomic>
#include <thread>

TEST(Concurrency, IndependentSerializersCanReadSharedStableObject) {
  auto* root = olib_object_new(OLIB_OBJECT_TYPE_STRUCT);
  auto* value = olib_object_new(OLIB_OBJECT_TYPE_STRING);
  ASSERT_NE(root, nullptr);
  ASSERT_NE(value, nullptr);
  ASSERT_TRUE(olib_object_set_string(value, "shared value"));
  ASSERT_TRUE(olib_object_struct_add(root, "name", value));

  std::atomic<bool> valid{true};
  std::array<std::thread, 4> workers;
  for (auto& worker : workers) {
    worker = std::thread([root, &valid]() {
      for (int iteration = 0; iteration < 100; ++iteration) {
        char* encoded = nullptr;
        if (!olib_format_write_string(OLIB_FORMAT_JSON_TEXT, root, &encoded) || !encoded) {
          valid = false;
          olib_free(encoded);
          return;
        }
        auto* decoded = olib_format_read_string(OLIB_FORMAT_JSON_TEXT, encoded);
        olib_free(encoded);
        auto* name = decoded ? olib_object_struct_get(decoded, "name") : nullptr;
        if (!decoded || olib_object_get_type(decoded) != OLIB_OBJECT_TYPE_STRUCT ||
            olib_object_struct_size(decoded) != 1 || !name ||
            olib_object_get_type(name) != OLIB_OBJECT_TYPE_STRING ||
            !olib_object_get_string(name) ||
            std::strcmp(olib_object_get_string(name), "shared value") != 0) {
          valid = false;
        }
        olib_object_free(decoded);
      }
    });
  }
  for (auto& worker : workers) worker.join();
  EXPECT_TRUE(valid.load());
  olib_object_free(root);
}
