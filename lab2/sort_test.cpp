#include "quick_sort.hpp"

#include <algorithm>
#include <random>
#include <string>
#include <vector>

#include <gtest/gtest.h>

class SortTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        rng.seed(42);
    }
    std::mt19937 rng;
};

TEST_F(SortTest, EmptyArray)
{
    std::vector<int> vec;
    jd::sort(vec.begin(), vec.end());
    EXPECT_TRUE(vec.empty());
}

TEST_F(SortTest, SingleElement)
{
    std::vector<int> vec = {42};
    jd::sort(vec.begin(), vec.end());
    EXPECT_EQ(vec, std::vector<int>({42}));
}

TEST_F(SortTest, AlreadySorted)
{
    std::vector<int> vec      = {1, 2, 3, 4, 5};
    std::vector<int> expected = vec;
    jd::sort(vec.begin(), vec.end());
    EXPECT_EQ(vec, expected);
}

TEST_F(SortTest, ReverseSorted)
{
    std::vector<int> vec      = {5, 4, 3, 2, 1};
    std::vector<int> expected = {1, 2, 3, 4, 5};
    jd::sort(vec.begin(), vec.end());
    EXPECT_EQ(vec, expected);
}

TEST_F(SortTest, RandomArray)
{
    std::vector<int> vec      = {3, 1, 4, 1, 5, 9, 2, 6};
    std::vector<int> expected = vec;
    std::sort(expected.begin(), expected.end());
    jd::sort(vec.begin(), vec.end());
    EXPECT_EQ(vec, expected);
}

TEST_F(SortTest, WithDuplicates)
{
    std::vector<int> vec      = {2, 2, 1, 1, 3, 3, 3, 2};
    std::vector<int> expected = {1, 1, 2, 2, 2, 3, 3, 3};
    jd::sort(vec.begin(), vec.end());
    EXPECT_EQ(vec, expected);
}

TEST_F(SortTest, DescendingOrder)
{
    std::vector<int> vec      = {1, 2, 3, 4, 5};
    std::vector<int> expected = {5, 4, 3, 2, 1};
    jd::sort(vec.begin(), vec.end(), std::greater<int>());
    EXPECT_EQ(vec, expected);
}

TEST_F(SortTest, LargeRandomArray)
{
    const int size = 1000;
    std::vector<int> vec(size);
    std::generate(vec.begin(), vec.end(), [this]() { return rng() % 1000; });
    std::vector<int> expected = vec;
    std::sort(expected.begin(), expected.end());
    jd::sort(vec.begin(), vec.end());
    EXPECT_EQ(vec, expected);
}

TEST_F(SortTest, StringArray)
{
    std::vector<std::string> vec      = {"banana", "apple", "cherry", "date"};
    std::vector<std::string> expected = {"apple", "banana", "cherry", "date"};
    jd::sort(vec.begin(), vec.end());
    EXPECT_EQ(vec, expected);
}

TEST_F(SortTest, CustomComparator)
{
    struct Point {
        int x, y;
        bool operator<(const Point& other) const
        {
            return x < other.x || (x == other.x && y < other.y);
        }
    };
    std::vector<Point> vec      = {{3, 1}, {1, 2}, {2, 3}, {1, 1}};
    std::vector<Point> expected = {{1, 1}, {1, 2}, {2, 3}, {3, 1}};
    jd::sort(vec.begin(), vec.end());
    for (size_t i = 0; i < vec.size(); ++i) {
        EXPECT_EQ(vec[i].x, expected[i].x);
        EXPECT_EQ(vec[i].y, expected[i].y);
    }
}

TEST_F(SortTest, Subarray)
{
    std::vector<int> vec = {9, 8, 7, 6, 5, 4, 3, 2, 1};
    jd::sort(vec.begin() + 2, vec.begin() + 7);
    std::vector<int> expected = {9, 8, 3, 4, 5, 6, 7, 2, 1};
    EXPECT_EQ(vec, expected);
}


TEST_F(SortTest, LambdaComparator)
{
    std::vector<int> vec = {5, 3, 1, 4, 2};
    auto cmp             = [](int a, int b) { return a > b; };
    jd::sort(vec.begin(), vec.end(), cmp);
    std::vector<int> expected = {5, 4, 3, 2, 1};
    EXPECT_EQ(vec, expected);
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
