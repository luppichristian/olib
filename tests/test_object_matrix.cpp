#include <gtest/gtest.h>
#include <olib.h>

TEST(ObjectMatrix, Create1DMatrix)
{
    size_t dims[] = {5};
    olib_object_t* mat = olib_object_matrix_new(1, dims);
    ASSERT_NE(mat, nullptr);

    EXPECT_EQ(olib_object_get_type(mat), OLIB_OBJECT_TYPE_MATRIX);
    EXPECT_EQ(olib_object_matrix_ndims(mat), 1u);
    EXPECT_EQ(olib_object_matrix_dim(mat, 0), 5u);
    EXPECT_EQ(olib_object_matrix_total_size(mat), 5u);

    olib_object_free(mat);
}

TEST(ObjectMatrix, Create2DMatrix)
{
    size_t dims[] = {3, 4};
    olib_object_t* mat = olib_object_matrix_new(2, dims);
    ASSERT_NE(mat, nullptr);

    EXPECT_EQ(olib_object_matrix_ndims(mat), 2u);
    EXPECT_EQ(olib_object_matrix_dim(mat, 0), 3u);
    EXPECT_EQ(olib_object_matrix_dim(mat, 1), 4u);
    EXPECT_EQ(olib_object_matrix_total_size(mat), 12u);

    olib_object_free(mat);
}

TEST(ObjectMatrix, Create3DMatrix)
{
    size_t dims[] = {2, 3, 4};
    olib_object_t* mat = olib_object_matrix_new(3, dims);
    ASSERT_NE(mat, nullptr);

    EXPECT_EQ(olib_object_matrix_ndims(mat), 3u);
    EXPECT_EQ(olib_object_matrix_dim(mat, 0), 2u);
    EXPECT_EQ(olib_object_matrix_dim(mat, 1), 3u);
    EXPECT_EQ(olib_object_matrix_dim(mat, 2), 4u);
    EXPECT_EQ(olib_object_matrix_total_size(mat), 24u);

    olib_object_free(mat);
}

TEST(ObjectMatrix, GetSetValues)
{
    size_t dims[] = {2, 3};
    olib_object_t* mat = olib_object_matrix_new(2, dims);

    // Set values
    for (size_t i = 0; i < 2; i++) {
        for (size_t j = 0; j < 3; j++) {
            size_t indices[] = {i, j};
            EXPECT_TRUE(olib_object_matrix_set(mat, indices, (double)(i * 3 + j)));
        }
    }

    // Get values
    for (size_t i = 0; i < 2; i++) {
        for (size_t j = 0; j < 3; j++) {
            size_t indices[] = {i, j};
            EXPECT_DOUBLE_EQ(olib_object_matrix_get(mat, indices), (double)(i * 3 + j));
        }
    }

    olib_object_free(mat);
}

TEST(ObjectMatrix, Fill)
{
    size_t dims[] = {3, 3};
    olib_object_t* mat = olib_object_matrix_new(2, dims);

    EXPECT_TRUE(olib_object_matrix_fill(mat, 7.5));

    for (size_t i = 0; i < 3; i++) {
        for (size_t j = 0; j < 3; j++) {
            size_t indices[] = {i, j};
            EXPECT_DOUBLE_EQ(olib_object_matrix_get(mat, indices), 7.5);
        }
    }

    olib_object_free(mat);
}

TEST(ObjectMatrix, DirectDataAccess)
{
    size_t dims[] = {2, 2};
    olib_object_t* mat = olib_object_matrix_new(2, dims);

    double* data = olib_object_matrix_data(mat);
    ASSERT_NE(data, nullptr);

    data[0] = 1.0;
    data[1] = 2.0;
    data[2] = 3.0;
    data[3] = 4.0;

    size_t idx00[] = {0, 0};
    size_t idx01[] = {0, 1};
    size_t idx10[] = {1, 0};
    size_t idx11[] = {1, 1};

    EXPECT_DOUBLE_EQ(olib_object_matrix_get(mat, idx00), 1.0);
    EXPECT_DOUBLE_EQ(olib_object_matrix_get(mat, idx01), 2.0);
    EXPECT_DOUBLE_EQ(olib_object_matrix_get(mat, idx10), 3.0);
    EXPECT_DOUBLE_EQ(olib_object_matrix_get(mat, idx11), 4.0);

    olib_object_free(mat);
}

TEST(ObjectMatrix, SetData)
{
    size_t dims[] = {2, 2};
    olib_object_t* mat = olib_object_matrix_new(2, dims);

    double new_data[] = {10.0, 20.0, 30.0, 40.0};
    EXPECT_TRUE(olib_object_matrix_set_data(mat, new_data, 4));

    size_t idx00[] = {0, 0};
    size_t idx11[] = {1, 1};
    EXPECT_DOUBLE_EQ(olib_object_matrix_get(mat, idx00), 10.0);
    EXPECT_DOUBLE_EQ(olib_object_matrix_get(mat, idx11), 40.0);

    olib_object_free(mat);
}

TEST(ObjectMatrix, GetDims)
{
    size_t dims[] = {4, 5, 6};
    olib_object_t* mat = olib_object_matrix_new(3, dims);

    const size_t* retrieved_dims = olib_object_matrix_dims(mat);
    ASSERT_NE(retrieved_dims, nullptr);

    EXPECT_EQ(retrieved_dims[0], 4u);
    EXPECT_EQ(retrieved_dims[1], 5u);
    EXPECT_EQ(retrieved_dims[2], 6u);

    olib_object_free(mat);
}
