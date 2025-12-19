#include <iostream>
#include <vector>

#include "allocator.hpp"
#include "memory.hpp"

using namespace jd::memory;

int main(void)
{
    std::cout << "Lab4" << std::endl;

    auto& allocator = MemoryAllocator::allocator();
    allocator.init();

    int* pi    = (int*)allocator.alloc(sizeof(int));
    double* pd = (double*)allocator.alloc(sizeof(double));
    int* pa    = (int*)allocator.alloc(10 * sizeof(int));

    char* medium_alloc = (char*)allocator.alloc(2_MB);
    char* large_alloc  = (char*)allocator.alloc(5_MB);
    char* m_alloc      = (char*)allocator.alloc(10_MB);

    std::vector<void*> vec(16, nullptr);

    for (size_t i = 0; i < vec.size(); i++) {
        void* ptr = allocator.alloc(5_MB);
        if (ptr == nullptr) {
            std::cout << "Vec failed alloc i=" << i << std::endl;
            break;
        }
        vec[i] = ptr;
    }

    for (void* ptr : vec) {
        if (ptr) {
            allocator.free(ptr);
        }
    }

    if (medium_alloc == nullptr) {
        std::cout << "Failed to alloc 2 MB" << std::endl;
    }

    if (large_alloc == nullptr) {
        std::cout << "Failed to alloc 5 MB" << std::endl;
    }

    if (m_alloc == nullptr) {
        std::cout << "Faled to alloc mem from the OS" << std::endl;
    }

    allocator.dumpStat();
    allocator.dumpBlocks();

    allocator.free(pa);
    allocator.free(pd);
    allocator.free(pi);

    allocator.free(m_alloc);
    allocator.free(large_alloc);
    allocator.free(medium_alloc);

    allocator.destroy();

    return EXIT_SUCCESS;
}
