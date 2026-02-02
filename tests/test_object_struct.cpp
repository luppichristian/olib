#include <gtest/gtest.h>
#include <olib.h>
#include <cstdio>

TEST(ObjectStruct, EmptyStruct)
{
    olib_object_t* obj = olib_object_new(OLIB_OBJECT_TYPE_STRUCT);
    ASSERT_NE(obj, nullptr);

    EXPECT_EQ(olib_object_struct_size(obj), 0u);
    EXPECT_FALSE(olib_object_struct_has(obj, "key"));
    EXPECT_EQ(olib_object_struct_get(obj, "key"), nullptr);

    olib_object_free(obj);
}

TEST(ObjectStruct, AddAndGet)
{
    olib_object_t* obj = olib_object_new(OLIB_OBJECT_TYPE_STRUCT);

    olib_object_t* val = olib_object_new(OLIB_OBJECT_TYPE_INT);
    olib_object_set_int(val, 42);

    EXPECT_TRUE(olib_object_struct_add(obj, "answer", val));
    EXPECT_EQ(olib_object_struct_size(obj), 1u);
    EXPECT_TRUE(olib_object_struct_has(obj, "answer"));
    EXPECT_EQ(olib_object_get_int(olib_object_struct_get(obj, "answer")), 42);

    olib_object_free(obj);
}

TEST(ObjectStruct, AddDuplicateKeyFails)
{
    olib_object_t* obj = olib_object_new(OLIB_OBJECT_TYPE_STRUCT);

    olib_object_t* val1 = olib_object_new(OLIB_OBJECT_TYPE_INT);
    olib_object_t* val2 = olib_object_new(OLIB_OBJECT_TYPE_INT);
    olib_object_set_int(val1, 1);
    olib_object_set_int(val2, 2);

    EXPECT_TRUE(olib_object_struct_add(obj, "key", val1));
    EXPECT_FALSE(olib_object_struct_add(obj, "key", val2));  // Should fail
    olib_object_free(val2);

    // Original value should still be there
    EXPECT_EQ(olib_object_get_int(olib_object_struct_get(obj, "key")), 1);

    olib_object_free(obj);
}

TEST(ObjectStruct, SetOverwrites)
{
    olib_object_t* obj = olib_object_new(OLIB_OBJECT_TYPE_STRUCT);

    olib_object_t* val1 = olib_object_new(OLIB_OBJECT_TYPE_INT);
    olib_object_t* val2 = olib_object_new(OLIB_OBJECT_TYPE_INT);
    olib_object_set_int(val1, 1);
    olib_object_set_int(val2, 2);

    EXPECT_TRUE(olib_object_struct_set(obj, "key", val1));
    EXPECT_TRUE(olib_object_struct_set(obj, "key", val2));  // Should succeed and overwrite

    EXPECT_EQ(olib_object_struct_size(obj), 1u);
    EXPECT_EQ(olib_object_get_int(olib_object_struct_get(obj, "key")), 2);

    olib_object_free(obj);
}

TEST(ObjectStruct, Remove)
{
    olib_object_t* obj = olib_object_new(OLIB_OBJECT_TYPE_STRUCT);

    olib_object_t* val = olib_object_new(OLIB_OBJECT_TYPE_INT);
    olib_object_struct_add(obj, "key", val);

    EXPECT_TRUE(olib_object_struct_has(obj, "key"));
    EXPECT_TRUE(olib_object_struct_remove(obj, "key"));
    EXPECT_FALSE(olib_object_struct_has(obj, "key"));
    EXPECT_EQ(olib_object_struct_size(obj), 0u);

    // Remove non-existent key should fail
    EXPECT_FALSE(olib_object_struct_remove(obj, "nonexistent"));

    olib_object_free(obj);
}

TEST(ObjectStruct, KeyAtAndValueAt)
{
    olib_object_t* obj = olib_object_new(OLIB_OBJECT_TYPE_STRUCT);

    olib_object_t* val1 = olib_object_new(OLIB_OBJECT_TYPE_INT);
    olib_object_t* val2 = olib_object_new(OLIB_OBJECT_TYPE_STRING);
    olib_object_set_int(val1, 42);
    olib_object_set_string(val2, "hello");

    olib_object_struct_add(obj, "number", val1);
    olib_object_struct_add(obj, "text", val2);

    // Order may not be guaranteed, but we can check consistency
    EXPECT_EQ(olib_object_struct_size(obj), 2u);

    const char* key0 = olib_object_struct_key_at(obj, 0);
    const char* key1 = olib_object_struct_key_at(obj, 1);
    olib_object_t* val_at_0 = olib_object_struct_value_at(obj, 0);
    olib_object_t* val_at_1 = olib_object_struct_value_at(obj, 1);

    ASSERT_NE(key0, nullptr);
    ASSERT_NE(key1, nullptr);
    ASSERT_NE(val_at_0, nullptr);
    ASSERT_NE(val_at_1, nullptr);

    // Verify key and value match
    EXPECT_EQ(olib_object_struct_get(obj, key0), val_at_0);
    EXPECT_EQ(olib_object_struct_get(obj, key1), val_at_1);

    // Out of bounds
    EXPECT_EQ(olib_object_struct_key_at(obj, 2), nullptr);
    EXPECT_EQ(olib_object_struct_value_at(obj, 2), nullptr);

    olib_object_free(obj);
}

TEST(ObjectStruct, MultipleKeys)
{
    olib_object_t* obj = olib_object_new(OLIB_OBJECT_TYPE_STRUCT);

    for (int i = 0; i < 10; i++) {
        char key[32];
        snprintf(key, sizeof(key), "key%d", i);
        olib_object_t* val = olib_object_new(OLIB_OBJECT_TYPE_INT);
        olib_object_set_int(val, i * 10);
        olib_object_struct_add(obj, key, val);
    }

    EXPECT_EQ(olib_object_struct_size(obj), 10u);

    for (int i = 0; i < 10; i++) {
        char key[32];
        snprintf(key, sizeof(key), "key%d", i);
        EXPECT_TRUE(olib_object_struct_has(obj, key));
        EXPECT_EQ(olib_object_get_int(olib_object_struct_get(obj, key)), i * 10);
    }

    olib_object_free(obj);
}
