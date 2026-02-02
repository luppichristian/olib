#include <gtest/gtest.h>
#include <olib.h>

TEST(ObjectArray, EmptyArray)
{
    olib_object_t* arr = olib_object_new(OLIB_OBJECT_TYPE_ARRAY);
    ASSERT_NE(arr, nullptr);

    EXPECT_EQ(olib_object_array_size(arr), 0u);
    EXPECT_EQ(olib_object_array_get(arr, 0), nullptr);

    olib_object_free(arr);
}

TEST(ObjectArray, PushAndGet)
{
    olib_object_t* arr = olib_object_new(OLIB_OBJECT_TYPE_ARRAY);
    ASSERT_NE(arr, nullptr);

    olib_object_t* val1 = olib_object_new(OLIB_OBJECT_TYPE_INT);
    olib_object_t* val2 = olib_object_new(OLIB_OBJECT_TYPE_INT);
    olib_object_set_int(val1, 10);
    olib_object_set_int(val2, 20);

    EXPECT_TRUE(olib_object_array_push(arr, val1));
    EXPECT_TRUE(olib_object_array_push(arr, val2));

    EXPECT_EQ(olib_object_array_size(arr), 2u);
    EXPECT_EQ(olib_object_get_int(olib_object_array_get(arr, 0)), 10);
    EXPECT_EQ(olib_object_get_int(olib_object_array_get(arr, 1)), 20);

    olib_object_free(arr);
}

TEST(ObjectArray, Pop)
{
    olib_object_t* arr = olib_object_new(OLIB_OBJECT_TYPE_ARRAY);

    olib_object_t* val = olib_object_new(OLIB_OBJECT_TYPE_INT);
    olib_object_set_int(val, 42);
    olib_object_array_push(arr, val);

    EXPECT_EQ(olib_object_array_size(arr), 1u);
    EXPECT_TRUE(olib_object_array_pop(arr));
    EXPECT_EQ(olib_object_array_size(arr), 0u);

    // Pop on empty array should fail
    EXPECT_FALSE(olib_object_array_pop(arr));

    olib_object_free(arr);
}

TEST(ObjectArray, SetAtIndex)
{
    olib_object_t* arr = olib_object_new(OLIB_OBJECT_TYPE_ARRAY);

    olib_object_t* val1 = olib_object_new(OLIB_OBJECT_TYPE_INT);
    olib_object_set_int(val1, 10);
    olib_object_array_push(arr, val1);

    olib_object_t* val2 = olib_object_new(OLIB_OBJECT_TYPE_INT);
    olib_object_set_int(val2, 99);

    EXPECT_TRUE(olib_object_array_set(arr, 0, val2));
    EXPECT_EQ(olib_object_get_int(olib_object_array_get(arr, 0)), 99);

    // Set at invalid index should fail
    olib_object_t* val3 = olib_object_new(OLIB_OBJECT_TYPE_INT);
    EXPECT_FALSE(olib_object_array_set(arr, 100, val3));
    olib_object_free(val3);

    olib_object_free(arr);
}

TEST(ObjectArray, InsertAtIndex)
{
    olib_object_t* arr = olib_object_new(OLIB_OBJECT_TYPE_ARRAY);

    olib_object_t* val1 = olib_object_new(OLIB_OBJECT_TYPE_INT);
    olib_object_t* val2 = olib_object_new(OLIB_OBJECT_TYPE_INT);
    olib_object_t* val3 = olib_object_new(OLIB_OBJECT_TYPE_INT);
    olib_object_set_int(val1, 1);
    olib_object_set_int(val2, 3);
    olib_object_set_int(val3, 2);

    olib_object_array_push(arr, val1);
    olib_object_array_push(arr, val2);

    // Insert at index 1 (between val1 and val2)
    EXPECT_TRUE(olib_object_array_insert(arr, 1, val3));
    EXPECT_EQ(olib_object_array_size(arr), 3u);

    EXPECT_EQ(olib_object_get_int(olib_object_array_get(arr, 0)), 1);
    EXPECT_EQ(olib_object_get_int(olib_object_array_get(arr, 1)), 2);
    EXPECT_EQ(olib_object_get_int(olib_object_array_get(arr, 2)), 3);

    olib_object_free(arr);
}

TEST(ObjectArray, RemoveAtIndex)
{
    olib_object_t* arr = olib_object_new(OLIB_OBJECT_TYPE_ARRAY);

    for (int i = 0; i < 3; i++) {
        olib_object_t* val = olib_object_new(OLIB_OBJECT_TYPE_INT);
        olib_object_set_int(val, i);
        olib_object_array_push(arr, val);
    }

    EXPECT_EQ(olib_object_array_size(arr), 3u);

    // Remove middle element
    EXPECT_TRUE(olib_object_array_remove(arr, 1));
    EXPECT_EQ(olib_object_array_size(arr), 2u);
    EXPECT_EQ(olib_object_get_int(olib_object_array_get(arr, 0)), 0);
    EXPECT_EQ(olib_object_get_int(olib_object_array_get(arr, 1)), 2);

    // Remove at invalid index should fail
    EXPECT_FALSE(olib_object_array_remove(arr, 100));

    olib_object_free(arr);
}

TEST(ObjectArray, OutOfBoundsGet)
{
    olib_object_t* arr = olib_object_new(OLIB_OBJECT_TYPE_ARRAY);

    olib_object_t* val = olib_object_new(OLIB_OBJECT_TYPE_INT);
    olib_object_array_push(arr, val);

    EXPECT_EQ(olib_object_array_get(arr, 0), val);
    EXPECT_EQ(olib_object_array_get(arr, 1), nullptr);
    EXPECT_EQ(olib_object_array_get(arr, SIZE_MAX), nullptr);

    olib_object_free(arr);
}
