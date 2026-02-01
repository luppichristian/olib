#include <gtest/gtest.h>

extern "C" {
#include <olib.h>
}

TEST(OlibTest, AddPositiveNumbers)
{
    EXPECT_EQ(olib_add(2, 3), 5);
}

TEST(OlibTest, AddNegativeNumbers)
{
    EXPECT_EQ(olib_add(-2, -3), -5);
}

TEST(OlibTest, AddMixedNumbers)
{
    EXPECT_EQ(olib_add(-2, 5), 3);
}

TEST(OlibTest, AddZero)
{
    EXPECT_EQ(olib_add(0, 0), 0);
    EXPECT_EQ(olib_add(5, 0), 5);
    EXPECT_EQ(olib_add(0, 5), 5);
}
