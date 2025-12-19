#include "allocator.hpp"
#include "memory.hpp"

#include <cstring>
#include <gtest/gtest.h>
#include <random>

using namespace jd::memory;

namespace test
{

class MemoryAllocatorTest : public ::testing::Test
{
protected:
    MemoryAllocator& allocator{MemoryAllocator::allocator()};

    void SetUp() override
    {
        allocator.init();
    }

    void TearDown() override
    {
        allocator.destroy();
    }
};

TEST_F(MemoryAllocatorTest, BasicAllocation)
{
    int* pi    = static_cast<int*>(allocator.alloc(sizeof(int)));
    double* pd = static_cast<double*>(allocator.alloc(sizeof(double)));
    int* pa    = static_cast<int*>(allocator.alloc(10 * sizeof(int)));

    ASSERT_NE(pi, nullptr);
    ASSERT_NE(pd, nullptr);
    ASSERT_NE(pa, nullptr);

    *pi   = 42;
    *pd   = 3.14159;
    pa[0] = 1;
    pa[9] = 100;

    EXPECT_EQ(*pi, 42);
    EXPECT_DOUBLE_EQ(*pd, 3.14159);
    EXPECT_EQ(pa[0], 1);
    EXPECT_EQ(pa[9], 100);

    allocator.free(pa);
    allocator.free(pd);
    allocator.free(pi);
}

TEST_F(MemoryAllocatorTest, FSAAllocations)
{
    std::vector<void*> blocks;
    constexpr size_t sizes[] = {16, 32, 64, 128, 256, 512};

    for (size_t size : sizes) {
        void* block = allocator.alloc(size);
        ASSERT_NE(block, nullptr);
        blocks.push_back(block);

        if (size >= sizeof(uint32_t)) {
            *static_cast<uint32_t*>(block) = 0xDEADBEEF;
        }
    }

    for (size_t i = 0; i < blocks.size(); ++i) {
        size_t size = sizes[i];
        if (size >= sizeof(uint32_t)) {
            EXPECT_EQ(*static_cast<uint32_t*>(blocks[i]), 0xDEADBEEF);
        }
    }

    for (void* block : blocks) {
        allocator.free(block);
    }
}

TEST_F(MemoryAllocatorTest, FSABoundaries)
{
    for (size_t size : {1, 8, 15, 16}) {
        void* block = allocator.alloc(size);
        ASSERT_NE(block, nullptr);
        memset(block, 0xAA, size);
        allocator.free(block);
    }

    for (size_t size : {17, 31, 33, 63, 65, 127, 129, 255, 257, 511, 513}) {
        void* block = allocator.alloc(size);
        ASSERT_NE(block, nullptr);
        memset(block, 0xBB, std::min(size, static_cast<size_t>(1_KB)));
        allocator.free(block);
    }
}

TEST_F(MemoryAllocatorTest, CoalesceAllocations)
{
    const size_t sizes[] = {1024, 8192, 32768, 65536};

    for (size_t size : sizes) {
        void* block = allocator.alloc(size);
        ASSERT_NE(block, nullptr);

        memset(block, 0xCC, size);
        unsigned char* bytes = static_cast<unsigned char*>(block);
        EXPECT_EQ(bytes[0], 0xCC);
        EXPECT_EQ(bytes[size - 1], 0xCC);

        allocator.free(block);
    }
}

TEST_F(MemoryAllocatorTest, LargeAllocations)
{
    void* large1 = allocator.alloc(11_MB);
    void* large2 = allocator.alloc(20_MB);

    ASSERT_NE(large1, nullptr);
    ASSERT_NE(large2, nullptr);

    memset(large1, 0x11, 4096);
    memset(large2, 0x22, 4096);

    EXPECT_EQ(static_cast<unsigned char*>(large1)[0], 0x11);
    EXPECT_EQ(static_cast<unsigned char*>(large2)[0], 0x22);

    allocator.free(large2);
    allocator.free(large1);
}

TEST_F(MemoryAllocatorTest, FreeNullPointer)
{
    allocator.free(nullptr);

    SUCCEED();
}

TEST_F(MemoryAllocatorTest, ZeroSizeAllocation)
{
    void* block = allocator.alloc(0);
    EXPECT_EQ(block, nullptr);
}

TEST_F(MemoryAllocatorTest, Alignment)
{
    for (size_t size : {1, 7, 8, 9, 15, 16, 17, 31, 32, 100, 1000}) {
        void* block = allocator.alloc(size);
        ASSERT_NE(block, nullptr);

        uintptr_t address = reinterpret_cast<uintptr_t>(block);
        EXPECT_EQ(address % 8, 0) << "Pointer not aligned for size=" << size << "; address=" << address;

        allocator.free(block);
    }
}

TEST_F(MemoryAllocatorTest, RandomAllocationsStressTest)
{
    std::mt19937 gen(42);
    std::uniform_int_distribution<size_t> size_dist(1, 10000);
    std::uniform_int_distribution<int> op_dist(0, 1);

    const int NUM_OPERATIONS = 10000;
    struct Allocation {
        void* block;
        size_t size;
        int expected_value;
    };
    std::vector<Allocation> allocations;

    for (int i = 0; i < NUM_OPERATIONS; ++i) {
        if (op_dist(gen) == 0 || allocations.empty()) {
            size_t size = size_dist(gen);
            void* block = allocator.alloc(size);
            if (block != nullptr) {
                int expected_value = static_cast<int>(allocations.size());
                allocations.push_back({block, size, expected_value});

                if (size >= sizeof(int)) {
                    *static_cast<int*>(block) = expected_value;
                }
            }
        } else {
            size_t idx        = std::uniform_int_distribution<size_t>(0, allocations.size() - 1)(gen);
            Allocation& alloc = allocations[idx];

            if (alloc.size >= sizeof(int)) {
                EXPECT_EQ(*static_cast<int*>(alloc.block), alloc.expected_value) << "Data corruption detected at operation " << i << ", block index " << idx;
            }

            allocator.free(alloc.block);

            if (idx != allocations.size() - 1) {
                Allocation& last_alloc = allocations.back();

                if (last_alloc.size >= sizeof(int)) {
                    *static_cast<int*>(last_alloc.block) = static_cast<int>(idx);
                    last_alloc.expected_value            = static_cast<int>(idx);
                }
            }

            allocations[idx] = allocations.back();
            allocations.pop_back();
        }
    }

    for (const auto& alloc : allocations) {
        allocator.free(alloc.block);
    }
}
} // namespace test

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
