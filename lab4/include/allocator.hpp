#include <cstddef>

#include "allocator_stats.hpp"

#ifndef NDEBUG
#define ALLOCATOR_DEBUG 1
#include <unordered_map>
#else
#define ALLOCATOR_DEBUG 0
#endif

namespace jd::memory
{
struct free_node_t;
struct region_t;
struct free_list_t;
struct block_t;

class MemoryAllocator
{
public:
    static constexpr size_t FSA_SIZES_COUNT            = 6;
    static constexpr size_t FSA_SIZES[FSA_SIZES_COUNT] = {16, 32, 64, 128, 256, 512};
    static constexpr size_t COALESCE_LISTS_COUNT       = 3;

    // FSA pools - each pool manages blocks of fixed size
    struct FSAPool {
        size_t block_size{};
        free_list_t* free_list{nullptr};
        char* memory_pool{nullptr};
        size_t pool_size{};
        size_t used_blocks{};
    };

    // Coalesce allocation with multiple free lists
    struct CoalesceAllocator {
        region_t* regions{nullptr};
        free_node_t* free_lists[COALESCE_LISTS_COUNT] = {nullptr};
        size_t region_size;

        CoalesceAllocator(size_t size)
            : region_size{size}
        {
        }
    };

private:
    char* fsa_arena_start_{nullptr};
    char* fsa_arena_end_{nullptr};

    bool is_initialized_{false};

    FSAPool fsa_pools_[FSA_SIZES_COUNT];
    CoalesceAllocator coalesce_alloc_;

    Statistics stats_;
#if ALLOCATOR_DEBUG
    std::unordered_map<void*, size_t> large_allocs_map_;
#endif
public:
    ~MemoryAllocator();

    MemoryAllocator();
    MemoryAllocator(const MemoryAllocator&)            = delete;
    MemoryAllocator& operator=(const MemoryAllocator&) = delete;
    MemoryAllocator(MemoryAllocator&&)                 = default;
    MemoryAllocator& operator=(MemoryAllocator&&)      = default;

    void init();
    void destroy();

    void* alloc(size_t size);
    void free(void* p);

#if ALLOCATOR_DEBUG
    void dumpStat() const;
    void dumpBlocks() const;
#else
    void dumpStat() const {}
    void dumpBlocks() const {}
#endif
};
} // namespace jd::memory
