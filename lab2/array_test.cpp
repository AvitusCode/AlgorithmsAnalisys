#include "dynamic_array.hpp"
#include <gtest/gtest.h>
#include <string>

using namespace jd;

class ArrayTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        arr_.insert(1);
        arr_.insert(2);
        arr_.insert(3);
    }

    void TearDown() override {}

    Array<int> arr_ = Array<int>(3);
};

TEST_F(ArrayTest, BasicOperations)
{
    EXPECT_EQ(arr_.size(), 3);
    EXPECT_EQ(arr_.capacity(), 3);
    EXPECT_EQ(arr_[0], 1);
    EXPECT_EQ(arr_[1], 2);
    EXPECT_EQ(arr_[2], 3);
}

TEST_F(ArrayTest, InsertAtBeginning)
{
    arr_.insert(0, 0);
    EXPECT_EQ(arr_.size(), 4);
    EXPECT_EQ(arr_[0], 0);
    EXPECT_EQ(arr_[1], 1);
}

TEST_F(ArrayTest, InsertAtMiddle)
{
    arr_.insert(1, 15);
    EXPECT_EQ(arr_.size(), 4);
    EXPECT_EQ(arr_[0], 1);
    EXPECT_EQ(arr_[1], 15);
    EXPECT_EQ(arr_[2], 2);
}

TEST_F(ArrayTest, InsertAtEnd)
{
    arr_.insert(arr_.size(), 4);
    EXPECT_EQ(arr_.size(), 4);
    EXPECT_EQ(arr_[3], 4);
}

TEST_F(ArrayTest, RemoveFromBeginning)
{
    arr_.remove(0);
    EXPECT_EQ(arr_.size(), 2);
    EXPECT_EQ(arr_[0], 2);
    EXPECT_EQ(arr_[1], 3);
}

TEST_F(ArrayTest, RemoveFromMiddle)
{
    arr_.remove(1);
    EXPECT_EQ(arr_.size(), 2);
    EXPECT_EQ(arr_[0], 1);
    EXPECT_EQ(arr_[1], 3);
}

TEST_F(ArrayTest, RemoveFromEnd)
{
    arr_.remove(2);
    EXPECT_EQ(arr_.size(), 2);
    EXPECT_EQ(arr_[0], 1);
    EXPECT_EQ(arr_[1], 2);
}

TEST_F(ArrayTest, CopyConstructor)
{
    Array<int> copy = arr_;
    EXPECT_EQ(copy.size(), 3);
    EXPECT_EQ(copy[0], 1);
    EXPECT_EQ(copy[1], 2);
    EXPECT_EQ(copy[2], 3);
}

TEST_F(ArrayTest, CopyAssignment)
{
    Array<int> copy;
    copy = arr_;
    EXPECT_EQ(copy.size(), 3);
    EXPECT_EQ(copy[0], 1);
    EXPECT_EQ(copy[1], 2);
    EXPECT_EQ(copy[2], 3);
}

TEST_F(ArrayTest, MoveConstructor)
{
    Array<int> moved = std::move(arr_);
    EXPECT_EQ(moved.size(), 3);
    EXPECT_EQ(moved[0], 1);
    EXPECT_EQ(moved[1], 2);
    EXPECT_EQ(moved[2], 3);
    EXPECT_EQ(arr_.size(), 0);
}

TEST_F(ArrayTest, MoveAssignment)
{
    Array<int> moved;
    moved = std::move(arr_);
    EXPECT_EQ(moved.size(), 3);
    EXPECT_EQ(moved[0], 1);
    EXPECT_EQ(moved[1], 2);
    EXPECT_EQ(moved[2], 3);
    EXPECT_EQ(arr_.size(), 0);
}

TEST_F(ArrayTest, Reallocation)
{
    int old_capacity = arr_.capacity();
    arr_.insert(33);
    EXPECT_GT(arr_.capacity(), old_capacity);
    EXPECT_EQ(arr_.size(), 4);
    EXPECT_EQ(arr_[0], 1);
    EXPECT_EQ(arr_[1], 2);
    EXPECT_EQ(arr_[2], 3);
    EXPECT_EQ(arr_[3], 33);
}

TEST(ArrayEmptyTest, EmptyArray)
{
    Array<int> arr;
    EXPECT_EQ(arr.size(), 0);
    EXPECT_GE(arr.capacity(), 16);
}

TEST(ArrayEmptyTest, SingleElement)
{
    Array<int> arr;
    arr.insert(42);
    EXPECT_EQ(arr.size(), 1);
    EXPECT_EQ(arr[0], 42);

    arr.remove(0);
    EXPECT_EQ(arr.size(), 0);
}

struct TestStruct {
    int id;
    std::string name;
    TestStruct(int i = 0, std::string n = "")
        : id{i}
        , name{std::move(n)}
    {
    }

    bool operator==(const TestStruct& other) const
    {
        return id == other.id && name == other.name;
    }
};

TEST(ArrayCustomTypeTest, CustomTypeOperations)
{
    Array<TestStruct> arr;
    arr.insert(TestStruct{1, "first"});
    arr.insert(TestStruct{2, "second"});

    EXPECT_EQ(arr.size(), 2);
    EXPECT_EQ(arr[0].id, 1);
    EXPECT_EQ(arr[1].name, "second");
}

TEST_F(ArrayTest, ForwardIterator)
{
    std::vector<int> result;
    for (int num : arr_) {
        result.push_back(num);
    }
    EXPECT_EQ(result, std::vector<int>({1, 2, 3}));
}

TEST_F(ArrayTest, ReverseIterator)
{
    std::vector<int> result;
    for (auto it = arr_.rbegin(); it != arr_.rend(); ++it) {
        result.push_back(*it);
    }
    EXPECT_EQ(result, std::vector<int>({3, 2, 1}));
}

TEST(ArrayIteratorTest, IteratorMethods)
{
    Array<int> arr;
    arr.insert(10);
    arr.insert(20);

    auto it = arr.begin();
    EXPECT_EQ(it.get(), 10);
    EXPECT_TRUE(it.hasNext());

    it.next();
    EXPECT_EQ(it.get(), 20);

    it.set(25);
    EXPECT_EQ(arr[1], 25);
}

TEST(ArrayLargeTest, LargeData)
{
    Array<int> arr;
    const int COUNT = 1000;

    for (int i = 0; i < COUNT; ++i) {
        arr.insert(i);
    }

    EXPECT_EQ(arr.size(), COUNT);
    for (int i = 0; i < COUNT; ++i) {
        EXPECT_EQ(arr[i], i);
    }
}

TEST(ArrayEdgeCasesTest, InsertRemoveSequence)
{
    Array<int> arr;

    arr.insert(0, 100);
    EXPECT_EQ(arr.size(), 1);

    arr.insert(arr.size(), 200);
    EXPECT_EQ(arr.size(), 2);

    arr.remove(0);
    EXPECT_EQ(arr.size(), 1);

    arr.remove(0);
    EXPECT_EQ(arr.size(), 0);
}

TEST_F(ArrayTest, SetInForwardIterator)
{
    auto it = arr_.begin();
    it.set(10);
    EXPECT_EQ(arr_[0], 10);
    EXPECT_EQ(arr_[1], 2);
    EXPECT_EQ(arr_[2], 3);

    ++it;
    it.set(20);
    EXPECT_EQ(arr_[0], 10);
    EXPECT_EQ(arr_[1], 20);
    EXPECT_EQ(arr_[2], 3);

    ++it;
    it.set(30);
    EXPECT_EQ(arr_[0], 10);
    EXPECT_EQ(arr_[1], 20);
    EXPECT_EQ(arr_[2], 30);
}

TEST_F(ArrayTest, SetInReverseIterator)
{
    auto it = arr_.rbegin();
    it.set(30);
    EXPECT_EQ(arr_[0], 1);
    EXPECT_EQ(arr_[1], 2);
    EXPECT_EQ(arr_[2], 30);

    ++it;
    it.set(20);
    EXPECT_EQ(arr_[0], 1);
    EXPECT_EQ(arr_[1], 20);
    EXPECT_EQ(arr_[2], 30);

    ++it;
    it.set(10);
    EXPECT_EQ(arr_[0], 10);
    EXPECT_EQ(arr_[1], 20);
    EXPECT_EQ(arr_[2], 30);
}

TEST(ArrayIteratorSetTest, SetWithCustomType)
{
    Array<TestStruct> arr;
    arr.insert(TestStruct{1, "first"});
    arr.insert(TestStruct{2, "second"});

    auto it = arr.begin();
    it.set(TestStruct{10, "modified"});
    EXPECT_EQ(arr[0].id, 10);
    EXPECT_EQ(arr[0].name, "modified");
    EXPECT_EQ(arr[1].id, 2);
    EXPECT_EQ(arr[1].name, "second");
}

TEST_F(ArrayTest, SetDoesNotChangeSize)
{
    auto it = arr_.begin();
    it.set(10);

    EXPECT_EQ(arr_.size(), 3);
    EXPECT_EQ(arr_[0], 10);
    EXPECT_EQ(arr_[1], 2);
    EXPECT_EQ(arr_[2], 3);
}

TEST_F(ArrayTest, SetAfterMultipleOperations)
{
    auto it = arr_.begin();
    it.set(10);
    ++it;
    it.set(20);
    it.next();
    it.set(30);

    EXPECT_EQ(arr_[0], 10);
    EXPECT_EQ(arr_[1], 20);
    EXPECT_EQ(arr_[2], 30);
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
