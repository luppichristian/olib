#include <gtest/gtest.h>
#include <olib.h>

TEST(ObjectCreation, CreateAllTypes)
{
    olib_object_t* obj_struct = olib_object_new(OLIB_OBJECT_TYPE_STRUCT);
    olib_object_t* obj_array = olib_object_new(OLIB_OBJECT_TYPE_ARRAY);
    olib_object_t* obj_int = olib_object_new(OLIB_OBJECT_TYPE_INT);
    olib_object_t* obj_uint = olib_object_new(OLIB_OBJECT_TYPE_UINT);
    olib_object_t* obj_float = olib_object_new(OLIB_OBJECT_TYPE_FLOAT);
    olib_object_t* obj_string = olib_object_new(OLIB_OBJECT_TYPE_STRING);
    olib_object_t* obj_bool = olib_object_new(OLIB_OBJECT_TYPE_BOOL);

    ASSERT_NE(obj_struct, nullptr);
    ASSERT_NE(obj_array, nullptr);
    ASSERT_NE(obj_int, nullptr);
    ASSERT_NE(obj_uint, nullptr);
    ASSERT_NE(obj_float, nullptr);
    ASSERT_NE(obj_string, nullptr);
    ASSERT_NE(obj_bool, nullptr);

    olib_object_free(obj_struct);
    olib_object_free(obj_array);
    olib_object_free(obj_int);
    olib_object_free(obj_uint);
    olib_object_free(obj_float);
    olib_object_free(obj_string);
    olib_object_free(obj_bool);
}

TEST(ObjectCreation, TypeInspection)
{
    olib_object_t* obj_struct = olib_object_new(OLIB_OBJECT_TYPE_STRUCT);
    olib_object_t* obj_array = olib_object_new(OLIB_OBJECT_TYPE_ARRAY);
    olib_object_t* obj_int = olib_object_new(OLIB_OBJECT_TYPE_INT);

    EXPECT_EQ(olib_object_get_type(obj_struct), OLIB_OBJECT_TYPE_STRUCT);
    EXPECT_EQ(olib_object_get_type(obj_array), OLIB_OBJECT_TYPE_ARRAY);
    EXPECT_EQ(olib_object_get_type(obj_int), OLIB_OBJECT_TYPE_INT);

    EXPECT_TRUE(olib_object_is_type(obj_struct, OLIB_OBJECT_TYPE_STRUCT));
    EXPECT_FALSE(olib_object_is_type(obj_struct, OLIB_OBJECT_TYPE_ARRAY));

    olib_object_free(obj_struct);
    olib_object_free(obj_array);
    olib_object_free(obj_int);
}

TEST(ObjectCreation, IsValueVsContainer)
{
    olib_object_t* obj_struct = olib_object_new(OLIB_OBJECT_TYPE_STRUCT);
    olib_object_t* obj_array = olib_object_new(OLIB_OBJECT_TYPE_ARRAY);
    olib_object_t* obj_int = olib_object_new(OLIB_OBJECT_TYPE_INT);
    olib_object_t* obj_string = olib_object_new(OLIB_OBJECT_TYPE_STRING);

    EXPECT_TRUE(olib_object_is_container(obj_struct));
    EXPECT_TRUE(olib_object_is_container(obj_array));
    EXPECT_FALSE(olib_object_is_container(obj_int));
    EXPECT_FALSE(olib_object_is_container(obj_string));

    EXPECT_FALSE(olib_object_is_value(obj_struct));
    EXPECT_FALSE(olib_object_is_value(obj_array));
    EXPECT_TRUE(olib_object_is_value(obj_int));
    EXPECT_TRUE(olib_object_is_value(obj_string));

    olib_object_free(obj_struct);
    olib_object_free(obj_array);
    olib_object_free(obj_int);
    olib_object_free(obj_string);
}

TEST(ObjectCreation, TypeToString)
{
    EXPECT_STREQ(olib_object_type_to_string(OLIB_OBJECT_TYPE_STRUCT), "struct");
    EXPECT_STREQ(olib_object_type_to_string(OLIB_OBJECT_TYPE_ARRAY), "array");
    EXPECT_STREQ(olib_object_type_to_string(OLIB_OBJECT_TYPE_INT), "int");
    EXPECT_STREQ(olib_object_type_to_string(OLIB_OBJECT_TYPE_UINT), "uint");
    EXPECT_STREQ(olib_object_type_to_string(OLIB_OBJECT_TYPE_FLOAT), "float");
    EXPECT_STREQ(olib_object_type_to_string(OLIB_OBJECT_TYPE_STRING), "string");
    EXPECT_STREQ(olib_object_type_to_string(OLIB_OBJECT_TYPE_BOOL), "bool");
    EXPECT_STREQ(olib_object_type_to_string(OLIB_OBJECT_TYPE_MATRIX), "matrix");
}

TEST(ObjectCreation, FreeNullIsSafe)
{
    olib_object_free(nullptr);  // Should not crash
}
