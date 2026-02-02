#include <cmath>
#include "test_utils.h"

// =============================================================================
// Helper Functions for Validation
// =============================================================================

// Helper to verify example1 structure
static void verify_example1_object(olib_object_t* obj) {
  ASSERT_NE(obj, nullptr);
  ASSERT_EQ(olib_object_get_type(obj), OLIB_OBJECT_TYPE_STRUCT);

  // Verify int_value
  olib_object_t* int_val = olib_object_struct_get(obj, "int_value");
  ASSERT_NE(int_val, nullptr);
  EXPECT_EQ(olib_object_get_int(int_val), -42);

  // Verify uint_value
  olib_object_t* uint_val = olib_object_struct_get(obj, "uint_value");
  ASSERT_NE(uint_val, nullptr);
  EXPECT_EQ(olib_object_get_uint(uint_val), 12345u);

  // Verify float_value
  olib_object_t* float_val = olib_object_struct_get(obj, "float_value");
  ASSERT_NE(float_val, nullptr);
  EXPECT_NEAR(olib_object_get_float(float_val), 3.14159, 0.00001);

  // Verify string_value
  olib_object_t* string_val = olib_object_struct_get(obj, "string_value");
  ASSERT_NE(string_val, nullptr);
  EXPECT_STREQ(olib_object_get_string(string_val), "Hello, World!");

  // Verify bool_value
  olib_object_t* bool_val = olib_object_struct_get(obj, "bool_value");
  ASSERT_NE(bool_val, nullptr);
  EXPECT_TRUE(olib_object_get_bool(bool_val));

  // Verify list_simple
  olib_object_t* list_simple = olib_object_struct_get(obj, "list_simple");
  ASSERT_NE(list_simple, nullptr);
  ASSERT_EQ(olib_object_get_type(list_simple), OLIB_OBJECT_TYPE_LIST);
  EXPECT_EQ(olib_object_list_size(list_simple), 3u);
  EXPECT_EQ(olib_object_get_int(olib_object_list_get(list_simple, 0)), 100);
  EXPECT_EQ(olib_object_get_int(olib_object_list_get(list_simple, 1)), 200);
  EXPECT_EQ(olib_object_get_int(olib_object_list_get(list_simple, 2)), 300);

  // Verify list_mixed
  olib_object_t* list_mixed = olib_object_struct_get(obj, "list_mixed");
  ASSERT_NE(list_mixed, nullptr);
  ASSERT_EQ(olib_object_get_type(list_mixed), OLIB_OBJECT_TYPE_LIST);
  EXPECT_EQ(olib_object_list_size(list_mixed), 2u);

  // First element of list_mixed
  olib_object_t* person1 = olib_object_list_get(list_mixed, 0);
  ASSERT_NE(person1, nullptr);
  ASSERT_EQ(olib_object_get_type(person1), OLIB_OBJECT_TYPE_STRUCT);
  EXPECT_STREQ(olib_object_get_string(olib_object_struct_get(person1, "name")), "Alice");
  EXPECT_EQ(olib_object_get_int(olib_object_struct_get(person1, "age")), 30);

  // Second element of list_mixed
  olib_object_t* person2 = olib_object_list_get(list_mixed, 1);
  ASSERT_NE(person2, nullptr);
  ASSERT_EQ(olib_object_get_type(person2), OLIB_OBJECT_TYPE_STRUCT);
  EXPECT_STREQ(olib_object_get_string(olib_object_struct_get(person2, "name")), "Bob");
  EXPECT_EQ(olib_object_get_int(olib_object_struct_get(person2, "age")), 25);

  // Verify nested_struct
  olib_object_t* nested = olib_object_struct_get(obj, "nested_struct");
  ASSERT_NE(nested, nullptr);
  ASSERT_EQ(olib_object_get_type(nested), OLIB_OBJECT_TYPE_STRUCT);
  EXPECT_EQ(olib_object_get_int(olib_object_struct_get(nested, "nested_int")), 999);
  EXPECT_NEAR(olib_object_get_float(olib_object_struct_get(nested, "nested_float")), 2.71828, 0.00001);
  EXPECT_FALSE(olib_object_get_bool(olib_object_struct_get(nested, "nested_bool")));
  EXPECT_STREQ(olib_object_get_string(olib_object_struct_get(nested, "nested_string")), "Nested value");
}

// Helper to verify example2 structure
static void verify_example2_object(olib_object_t* obj) {
  ASSERT_NE(obj, nullptr);
  ASSERT_EQ(olib_object_get_type(obj), OLIB_OBJECT_TYPE_STRUCT);

  // Verify person struct
  olib_object_t* person = olib_object_struct_get(obj, "person");
  ASSERT_NE(person, nullptr);
  ASSERT_EQ(olib_object_get_type(person), OLIB_OBJECT_TYPE_STRUCT);
  EXPECT_STREQ(olib_object_get_string(olib_object_struct_get(person, "name")), "John Doe");
  EXPECT_EQ(olib_object_get_int(olib_object_struct_get(person, "age")), 35);
  EXPECT_NEAR(olib_object_get_float(olib_object_struct_get(person, "height")), 1.85, 0.001);
  EXPECT_TRUE(olib_object_get_bool(olib_object_struct_get(person, "is_active")));
  EXPECT_STREQ(olib_object_get_string(olib_object_struct_get(person, "email")), "john.doe@example.com");

  // Verify numbers struct
  olib_object_t* numbers = olib_object_struct_get(obj, "numbers");
  ASSERT_NE(numbers, nullptr);
  ASSERT_EQ(olib_object_get_type(numbers), OLIB_OBJECT_TYPE_STRUCT);

  // Check int_min and int_max
  olib_object_t* int_min = olib_object_struct_get(numbers, "int_min");
  ASSERT_NE(int_min, nullptr);
  EXPECT_EQ(olib_object_get_int(int_min), -9223372036854775807LL - 1);  // INT64_MIN

  olib_object_t* int_max = olib_object_struct_get(numbers, "int_max");
  ASSERT_NE(int_max, nullptr);
  EXPECT_EQ(olib_object_get_int(int_max), 9223372036854775807LL);  // INT64_MAX

  olib_object_t* uint_max = olib_object_struct_get(numbers, "uint_max");
  ASSERT_NE(uint_max, nullptr);
  EXPECT_EQ(olib_object_get_uint(uint_max), 18446744073709551615ULL);  // UINT64_MAX

  EXPECT_NEAR(olib_object_get_float(olib_object_struct_get(numbers, "float_pi")), 3.141592653589793, 0.0000000000001);
  EXPECT_NEAR(olib_object_get_float(olib_object_struct_get(numbers, "float_e")), 2.718281828459045, 0.0000000000001);

  // Verify flags struct
  olib_object_t* flags = olib_object_struct_get(obj, "flags");
  ASSERT_NE(flags, nullptr);
  ASSERT_EQ(olib_object_get_type(flags), OLIB_OBJECT_TYPE_STRUCT);
  EXPECT_TRUE(olib_object_get_bool(olib_object_struct_get(flags, "enabled")));
  EXPECT_FALSE(olib_object_get_bool(olib_object_struct_get(flags, "disabled")));
  EXPECT_TRUE(olib_object_get_bool(olib_object_struct_get(flags, "active")));

  // Verify data_list
  olib_object_t* data_list = olib_object_struct_get(obj, "data_list");
  ASSERT_NE(data_list, nullptr);
  ASSERT_EQ(olib_object_get_type(data_list), OLIB_OBJECT_TYPE_LIST);
  EXPECT_EQ(olib_object_list_size(data_list), 8u);
  int expected_fib[] = {1, 2, 3, 5, 8, 13, 21, 34};
  for (size_t i = 0; i < 8; i++) {
    EXPECT_EQ(olib_object_get_int(olib_object_list_get(data_list, i)), expected_fib[i]);
  }

  // Verify string_list
  olib_object_t* string_list = olib_object_struct_get(obj, "string_list");
  ASSERT_NE(string_list, nullptr);
  ASSERT_EQ(olib_object_get_type(string_list), OLIB_OBJECT_TYPE_LIST);
  EXPECT_EQ(olib_object_list_size(string_list), 3u);
  EXPECT_STREQ(olib_object_get_string(olib_object_list_get(string_list, 0)), "red");
  EXPECT_STREQ(olib_object_get_string(olib_object_list_get(string_list, 1)), "green");
  EXPECT_STREQ(olib_object_get_string(olib_object_list_get(string_list, 2)), "blue");

  // Verify empty_list
  olib_object_t* empty_list = olib_object_struct_get(obj, "empty_list");
  ASSERT_NE(empty_list, nullptr);
  ASSERT_EQ(olib_object_get_type(empty_list), OLIB_OBJECT_TYPE_LIST);
  EXPECT_EQ(olib_object_list_size(empty_list), 0u);

  // Verify empty_struct
  olib_object_t* empty_struct = olib_object_struct_get(obj, "empty_struct");
  ASSERT_NE(empty_struct, nullptr);
  ASSERT_EQ(olib_object_get_type(empty_struct), OLIB_OBJECT_TYPE_STRUCT);
}

// =============================================================================
// JSON Tests - Example 1
// =============================================================================

TEST(SampleFiles, JsonExample1) {
  olib_serializer_t* ser = olib_serializer_new_json_text();
  ASSERT_NE(ser, nullptr);

  olib_object_t* obj = olib_serializer_read_file_path(ser, "samples/example1.json");
  verify_example1_object(obj);

  olib_object_free(obj);
  olib_serializer_free(ser);
}

// =============================================================================
// JSON Tests - Example 2
// =============================================================================

TEST(SampleFiles, JsonExample2) {
  olib_serializer_t* ser = olib_serializer_new_json_text();
  ASSERT_NE(ser, nullptr);

  olib_object_t* obj = olib_serializer_read_file_path(ser, "samples/example2.json");
  verify_example2_object(obj);

  olib_object_free(obj);
  olib_serializer_free(ser);
}

// =============================================================================
// TOML Tests - Example 1
// =============================================================================

TEST(SampleFiles, TomlExample1) {
  olib_serializer_t* ser = olib_serializer_new_toml();
  ASSERT_NE(ser, nullptr);

  olib_object_t* obj = olib_serializer_read_file_path(ser, "samples/example1.toml");
  verify_example1_object(obj);

  olib_object_free(obj);
  olib_serializer_free(ser);
}

// =============================================================================
// TOML Tests - Example 2
// =============================================================================

TEST(SampleFiles, TomlExample2) {
  olib_serializer_t* ser = olib_serializer_new_toml();
  ASSERT_NE(ser, nullptr);

  olib_object_t* obj = olib_serializer_read_file_path(ser, "samples/example2.toml");
  verify_example2_object(obj);

  olib_object_free(obj);
  olib_serializer_free(ser);
}

// =============================================================================
// Text Tests - Example 1
// =============================================================================

TEST(SampleFiles, TextExample1) {
  olib_serializer_t* ser = olib_serializer_new_txt();
  ASSERT_NE(ser, nullptr);

  olib_object_t* obj = olib_serializer_read_file_path(ser, "samples/example1.txt");
  verify_example1_object(obj);

  olib_object_free(obj);
  olib_serializer_free(ser);
}

// =============================================================================
// Text Tests - Example 2
// =============================================================================

TEST(SampleFiles, TextExample2) {
  olib_serializer_t* ser = olib_serializer_new_txt();
  ASSERT_NE(ser, nullptr);

  olib_object_t* obj = olib_serializer_read_file_path(ser, "samples/example2.txt");
  verify_example2_object(obj);

  olib_object_free(obj);
  olib_serializer_free(ser);
}

// =============================================================================
// XML Tests - Example 1
// =============================================================================

TEST(SampleFiles, XmlExample1) {
  olib_serializer_t* ser = olib_serializer_new_xml();
  ASSERT_NE(ser, nullptr);

  olib_object_t* obj = olib_serializer_read_file_path(ser, "samples/example1.xml");
  verify_example1_object(obj);

  olib_object_free(obj);
  olib_serializer_free(ser);
}

// =============================================================================
// XML Tests - Example 2
// =============================================================================

TEST(SampleFiles, XmlExample2) {
  olib_serializer_t* ser = olib_serializer_new_xml();
  ASSERT_NE(ser, nullptr);

  olib_object_t* obj = olib_serializer_read_file_path(ser, "samples/example2.xml");
  verify_example2_object(obj);

  olib_object_free(obj);
  olib_serializer_free(ser);
}

// =============================================================================
// YAML Tests - Example 1
// =============================================================================

TEST(SampleFiles, YamlExample1) {
  olib_serializer_t* ser = olib_serializer_new_yaml();
  ASSERT_NE(ser, nullptr);

  olib_object_t* obj = olib_serializer_read_file_path(ser, "samples/example1.yaml");
  verify_example1_object(obj);

  olib_object_free(obj);
  olib_serializer_free(ser);
}

// =============================================================================
// YAML Tests - Example 2
// =============================================================================

TEST(SampleFiles, YamlExample2) {
  olib_serializer_t* ser = olib_serializer_new_yaml();
  ASSERT_NE(ser, nullptr);

  olib_object_t* obj = olib_serializer_read_file_path(ser, "samples/example2.yaml");
  verify_example2_object(obj);

  olib_object_free(obj);
  olib_serializer_free(ser);
}
