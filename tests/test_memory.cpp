#include <gtest/gtest.h>
#include <olib.h>
#include <cstdlib>

static int g_malloc_count = 0;
static int g_free_count = 0;

static void* test_malloc(size_t size)
{
    g_malloc_count++;
    return malloc(size);
}

static void test_free(void* ptr)
{
    if (ptr) g_free_count++;
    free(ptr);
}

static void* test_calloc(size_t num, size_t size)
{
    g_malloc_count++;
    return calloc(num, size);
}

static void* test_realloc(void* ptr, size_t new_size)
{
    return realloc(ptr, new_size);
}

TEST(Memory, CustomAllocators)
{
    g_malloc_count = 0;
    g_free_count = 0;

    olib_set_memory_fns(test_malloc, test_free, test_calloc, test_realloc);

    olib_object_t* obj = olib_object_new(OLIB_OBJECT_TYPE_INT);
    olib_object_set_int(obj, 42);
    EXPECT_GT(g_malloc_count, 0);

    olib_object_free(obj);
    EXPECT_GT(g_free_count, 0);

    // Reset to default allocators
    olib_set_memory_fns(nullptr, nullptr, nullptr, nullptr);
}

TEST(Memory, CustomAllocatorsWithStruct)
{
    g_malloc_count = 0;
    g_free_count = 0;

    olib_set_memory_fns(test_malloc, test_free, test_calloc, test_realloc);

    olib_object_t* obj = olib_object_new(OLIB_OBJECT_TYPE_STRUCT);

    olib_object_t* val1 = olib_object_new(OLIB_OBJECT_TYPE_INT);
    olib_object_set_int(val1, 42);
    olib_object_struct_add(obj, "key1", val1);

    olib_object_t* val2 = olib_object_new(OLIB_OBJECT_TYPE_STRING);
    olib_object_set_string(val2, "test string");
    olib_object_struct_add(obj, "key2", val2);

    int malloc_count_before_free = g_malloc_count;
    EXPECT_GT(malloc_count_before_free, 2);  // At least struct + 2 values

    olib_object_free(obj);
    EXPECT_GT(g_free_count, 0);

    // Reset to default allocators
    olib_set_memory_fns(nullptr, nullptr, nullptr, nullptr);
}

TEST(Memory, CustomAllocatorsWithArray)
{
    g_malloc_count = 0;
    g_free_count = 0;

    olib_set_memory_fns(test_malloc, test_free, test_calloc, test_realloc);

    olib_object_t* arr = olib_object_new(OLIB_OBJECT_TYPE_ARRAY);

    for (int i = 0; i < 5; i++) {
        olib_object_t* val = olib_object_new(OLIB_OBJECT_TYPE_INT);
        olib_object_set_int(val, i);
        olib_object_array_push(arr, val);
    }

    EXPECT_GT(g_malloc_count, 5);  // At least array + 5 values

    olib_object_free(arr);
    EXPECT_GT(g_free_count, 0);

    // Reset to default allocators
    olib_set_memory_fns(nullptr, nullptr, nullptr, nullptr);
}

TEST(Memory, DefaultAllocatorsWork)
{
    // Ensure default allocators work after reset
    olib_set_memory_fns(nullptr, nullptr, nullptr, nullptr);

    olib_object_t* obj = olib_object_new(OLIB_OBJECT_TYPE_STRING);
    ASSERT_NE(obj, nullptr);

    EXPECT_TRUE(olib_object_set_string(obj, "Test with default allocators"));
    EXPECT_STREQ(olib_object_get_string(obj), "Test with default allocators");

    olib_object_free(obj);
}
