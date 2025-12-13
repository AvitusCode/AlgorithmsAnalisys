#include "allocator.hpp"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sys/mman.h>
#include <unistd.h>

#include "memory.hpp"

namespace jd::memory
{
static constexpr size_t ALIGNMENT             = 8;
static constexpr size_t PAGE_SIZE             = 4_KB;
static constexpr size_t LARGE_ALLOC_THRESHOLD = 10_MB;

static constexpr size_t SMALL_REGION_MAX  = 10_KB;
static constexpr size_t MEDIUM_REGION_MAX = 1_MB;

static constexpr size_t REGION_SIZE          = 32_MB;
static constexpr size_t MAX_REGIONS          = 16;
static constexpr size_t REGION_COUNT_BY_TYPE = 3;
static constexpr size_t FSA_ARENA_SIZE       = 16_MB;
static constexpr size_t METADATA_SIZE        = 64_MB;
static constexpr size_t TOTAL_VIRTUAL_MEMORY = MAX_REGIONS * REGION_SIZE + FSA_ARENA_SIZE + METADATA_SIZE + PAGE_SIZE * 2;

struct free_list_t {
    free_list_t* next;
};

enum class RegionType : uint8_t { SMALL = 0, MEDIUM, LARGE };

struct alignas(ALIGNMENT) region_t {
    char* start;
    char* end;
    bool is_used;
    RegionType region_type;
};

struct alignas(ALIGNMENT) block_t {
    size_t current_size;
    size_t prev_size;
    free_node_t* free_node;
    bool is_free;

    block_t(size_t size, size_t prev, bool free)
        : current_size(size)
        , prev_size(prev)
        , free_node(nullptr)
        , is_free(free)
    {
    }
};

struct alignas(ALIGNMENT) free_node_t {
    free_node_t* next;
    free_node_t* prev;
    block_t* header;
    size_t list_index;

    free_node_t()
        : next(nullptr)
        , prev(nullptr)
        , header(nullptr)
        , list_index(0)
    {
    }
};

static_assert(sizeof(free_list_t) % ALIGNMENT == 0, "free_list_t not aligned");
static_assert(alignof(free_list_t) == ALIGNMENT, "free_list_t alignment wrong");
static_assert(sizeof(region_t) % ALIGNMENT == 0, "region_t not aligned");
static_assert(alignof(region_t) == ALIGNMENT, "region_t alignment wrong");
static_assert(sizeof(block_t) % ALIGNMENT == 0, "block_t not aligned");
static_assert(alignof(block_t) == ALIGNMENT, "block_t alignment wrong");
static_assert(sizeof(free_node_t) % ALIGNMENT == 0, "free_node_t not aligned");
static_assert(alignof(free_node_t) == ALIGNMENT, "free_node_t alignment wrong");

static char* g_virtual_memory         = nullptr;
static region_t* g_regions            = nullptr;
static free_node_t* g_free_nodes_pool = nullptr;
static free_node_t** g_free_lists     = nullptr;
static size_t g_free_nodes_used       = 0;
static size_t g_current_offset        = 0;
static size_t g_max_free_nodes        = 0;

inline constexpr size_t alignSize(size_t size) noexcept
{
    return (size + ALIGNMENT - 1) & ~(ALIGNMENT - 1);
}

inline constexpr size_t alignToPage(size_t size) noexcept
{
    return (size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
}

size_t getFSASizeClass(size_t size) noexcept
{
    for (size_t i = 0; i < MemoryAllocator::FSA_SIZES_COUNT; ++i) {
        if (size <= MemoryAllocator::FSA_SIZES[i]) {
            return i;
        }
    }
    return MemoryAllocator::FSA_SIZES_COUNT;
}

size_t getCoalesceListIndex(size_t size) noexcept
{
    if (size <= SMALL_REGION_MAX)
        return 0;
    if (size > SMALL_REGION_MAX && size <= MEDIUM_REGION_MAX)
        return 1;
    return 2;
}

RegionType getRegionType(size_t size) noexcept
{
    if (size <= SMALL_REGION_MAX)
        return RegionType::SMALL;
    if (size <= MEDIUM_REGION_MAX)
        return RegionType::MEDIUM;
    return RegionType::LARGE;
}

inline block_t* getBlockFromPointer(void* ptr) noexcept
{
    return reinterpret_cast<block_t*>(static_cast<char*>(ptr) - sizeof(block_t));
}

inline void* getPointerFromBlock(block_t* block) noexcept
{
    return reinterpret_cast<char*>(block) + sizeof(block_t);
}

block_t* getNextBlock(block_t* block, region_t* region) noexcept
{
    if (!block || !region)
        return nullptr;

    char* block_end = reinterpret_cast<char*>(block) + block->current_size;
    if (block_end >= region->end)
        return nullptr;

    return reinterpret_cast<block_t*>(block_end);
}

inline block_t* getPrevBlock(block_t* block) noexcept
{
    if (block->prev_size == 0) {
        return nullptr;
    }
    return reinterpret_cast<block_t*>(reinterpret_cast<char*>(block) - block->prev_size);
}

inline bool isBlockInRegion(block_t* block, region_t* region) noexcept
{
    return (reinterpret_cast<char*>(block) >= region->start && reinterpret_cast<char*>(block) < region->end);
}

[[nodiscard]] free_node_t* allocateFreeNode() noexcept
{
    if (g_free_nodes_used >= g_max_free_nodes) {
        return nullptr;
    }

    free_node_t* node = &g_free_nodes_pool[g_free_nodes_used++];

    node->next       = nullptr;
    node->prev       = nullptr;
    node->header     = nullptr;
    node->list_index = 0;

    return node;
}

void removeFromFreeList(free_node_t* node) noexcept
{
    if (!node) {
        return;
    }

    size_t list_index = node->list_index;

    if (node->prev) {
        node->prev->next = node->next;
    } else {
        g_free_lists[list_index] = node->next;
    }

    if (node->next) {
        node->next->prev = node->prev;
    }

    node->prev = node->next = nullptr;

    if (node->header) {
        node->header->free_node = nullptr;
    }
}

void addToFreeList(free_node_t* node, size_t list_index) noexcept
{
    if (!node || !node->header) {
        return;
    }

    node->list_index = list_index;

    free_node_t** list   = &g_free_lists[list_index];
    free_node_t* current = *list;
    free_node_t* prev    = nullptr;

    while (current && current->header && current->header->current_size < node->header->current_size) {
        prev    = current;
        current = current->next;
    }

    if (prev) {
        prev->next = node;
    } else {
        *list = node;
    }

    node->prev = prev;
    node->next = current;

    if (current) {
        current->prev = node;
    }

    node->header->free_node = node;
}

block_t* bestFitApproach(size_t size, size_t list_index) noexcept
{
    free_node_t* current  = g_free_lists[list_index];
    free_node_t* best_fit = nullptr;

    while (current) {
        if (current->header && current->header->is_free && current->header->current_size >= size) {
            if (!best_fit || current->header->current_size < best_fit->header->current_size) {
                best_fit = current;
            }
        }
        current = current->next;
    }

    return best_fit ? best_fit->header : nullptr;
}

region_t* findRegionForBlock(block_t* block) noexcept
{
    for (size_t i = 0; i < MAX_REGIONS; ++i) {
        if (g_regions[i].is_used && isBlockInRegion(block, &g_regions[i])) {
            return &g_regions[i];
        }
    }
    return nullptr;
}

region_t* findRegionForPointer(void* ptr) noexcept
{
    for (size_t i = 0; i < MAX_REGIONS; ++i) {
        if (g_regions[i].is_used && ptr >= g_regions[i].start && ptr < g_regions[i].end) {
            return &g_regions[i];
        }
    }
    return nullptr;
}

void mergeBlocks(block_t* first, block_t* second) noexcept
{
    if (!first || !second) {
        return;
    }

    char* first_end = reinterpret_cast<char*>(first) + first->current_size;
    if (first_end != reinterpret_cast<char*>(second)) {
        return;
    }

    first->current_size += second->current_size;

    region_t* region = findRegionForBlock(first);
    if (region) {
        block_t* next = getNextBlock(first, region);
        if (next) {
            next->prev_size = first->current_size;
        }
    }

    second->current_size = 0;
    second->prev_size    = 0;
    second->is_free      = false;
    if (second->free_node) {
        removeFromFreeList(second->free_node);
        second->free_node = nullptr;
    }
}

[[nodiscard]] region_t* allocateRegionByType(RegionType region_type) noexcept
{
    for (size_t i = 0; i < MAX_REGIONS; ++i) {
        if (g_regions[i].is_used) {
            continue;
        }
        static constexpr size_t usable_size = TOTAL_VIRTUAL_MEMORY - PAGE_SIZE * 2;
        if (g_current_offset + REGION_SIZE > usable_size) {
            return nullptr;
        }

        g_regions[i].start       = g_virtual_memory + PAGE_SIZE + g_current_offset;
        g_regions[i].end         = g_regions[i].start + REGION_SIZE;
        g_regions[i].is_used     = true;
        g_regions[i].region_type = region_type;

        g_current_offset += REGION_SIZE;
        return &g_regions[i];
    }
    return nullptr;
}

inline constexpr size_t getOptimalSplitSize(RegionType region_type, size_t remaining) noexcept
{
    switch (region_type) {
        case RegionType::SMALL:
            return alignSize(4_KB + sizeof(block_t));
        case RegionType::MEDIUM:
            return alignSize(64_KB + sizeof(block_t));
        case RegionType::LARGE: {
            if (remaining >= alignSize(10_MB + sizeof(block_t))) {
                return alignSize(10_MB + sizeof(block_t));
            } else if (remaining >= alignSize(5_MB + sizeof(block_t))) {
                return alignSize(5_MB + sizeof(block_t));
            } else if (remaining >= alignSize(2_MB + sizeof(block_t))) {
                return alignSize(2_MB + sizeof(block_t));
            } else if (remaining >= alignSize(1_MB + sizeof(block_t))) {
                return alignSize(1_MB + sizeof(block_t));
            } else {
                return alignSize(512_KB + sizeof(block_t));
            }
        }
        default:
            return alignSize(4_KB + sizeof(block_t));
    }
}

void initializeRegion(region_t* region) noexcept
{
    if (!region)
        return;

    RegionType region_type = region->region_type;
    char* current          = region->start;

    uintptr_t addr = reinterpret_cast<uintptr_t>(current);
    if (addr & (ALIGNMENT - 1)) {
        addr    = alignSize(addr);
        current = reinterpret_cast<char*>(addr);
    }

    size_t remaining       = region->end - current;
    size_t prev_block_size = 0;

    while (remaining > sizeof(block_t) + ALIGNMENT) {
        size_t target_block_size = getOptimalSplitSize(region_type, remaining);

        size_t block_size = (target_block_size <= remaining) ? target_block_size : alignSize(remaining);

        if (block_size < sizeof(block_t) + ALIGNMENT) {
            break;
        }

        block_t* block = ::new (current) block_t(block_size, prev_block_size, true);

        free_node_t* node = allocateFreeNode();
        if (!node) {
            break;
        }

        node->header      = block;
        size_t user_size  = block_size - sizeof(block_t);
        size_t list_index = getCoalesceListIndex(user_size);
        addToFreeList(node, list_index);

        prev_block_size = block_size;
        current += block_size;
        remaining -= block_size;

        if (region_type == RegionType::LARGE && prev_block_size >= alignSize(5_MB + sizeof(block_t))) {
            if (remaining < alignSize(5_MB + sizeof(block_t))) {
                if (remaining >= sizeof(block_t) + ALIGNMENT) {
                    size_t last_block_size = alignSize(remaining);
                    block_t* last_block    = ::new (current) block_t(last_block_size, prev_block_size, true);

                    free_node_t* last_node = allocateFreeNode();
                    if (last_node) {
                        last_node->header      = last_block;
                        size_t last_user_size  = last_block_size - sizeof(block_t);
                        size_t last_list_index = getCoalesceListIndex(last_user_size);
                        addToFreeList(last_node, last_list_index);
                    }
                }
                break;
            }
        }
    }

    if (remaining >= sizeof(block_t) + ALIGNMENT) {
        size_t block_size = alignSize(remaining);
        block_t* block    = ::new (current) block_t(block_size, prev_block_size, true);

        free_node_t* node = allocateFreeNode();
        if (node) {
            node->header      = block;
            size_t user_size  = block_size - sizeof(block_t);
            size_t list_index = getCoalesceListIndex(user_size);
            addToFreeList(node, list_index);
        }
    }
}

bool isPointerInCoalesceRegion(void* ptr) noexcept
{
    for (size_t i = 0; i < MAX_REGIONS; ++i) {
        if (g_regions[i].is_used && ptr >= g_regions[i].start && ptr < g_regions[i].end) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] void* allocateFromCoalesce(size_t size, Statistics& stats) noexcept
{
    if (size > LARGE_ALLOC_THRESHOLD) [[unlikely]] {
        return nullptr;
    }

    size_t total_size      = alignSize(size + sizeof(block_t));
    RegionType region_type = getRegionType(size);
    size_t list_index      = static_cast<size_t>(region_type);

    block_t* best_fit = bestFitApproach(total_size, list_index);

    for (size_t i = list_index + 1; !best_fit && i < MemoryAllocator::COALESCE_LISTS_COUNT; ++i) {
        best_fit = bestFitApproach(total_size, i);
    }

    if (!best_fit) {
        region_t* new_region = allocateRegionByType(static_cast<RegionType>(list_index));
        if (!new_region) {
            return nullptr;
        }

        initializeRegion(new_region);

        best_fit = bestFitApproach(total_size, list_index);
        for (size_t i = list_index + 1; !best_fit && i < MemoryAllocator::COALESCE_LISTS_COUNT; ++i) {
            best_fit = bestFitApproach(total_size, i);
        }
    }

    if (!best_fit) {
        return nullptr;
    }

    if (best_fit->free_node) {
        removeFromFreeList(best_fit->free_node);
        best_fit->free_node = nullptr;
    }

    best_fit->is_free = false;

    size_t remaining = best_fit->current_size - total_size;
    if (remaining >= sizeof(block_t) + ALIGNMENT) {
        size_t min_split_size = (region_type == RegionType::LARGE) ? alignSize(1_MB + sizeof(block_t)) : alignSize(4_KB + sizeof(block_t));

        if (remaining >= min_split_size) {
            size_t aligned_new_size = remaining & ~(ALIGNMENT - 1);

            if (aligned_new_size >= sizeof(block_t) + ALIGNMENT) {
                if (g_free_nodes_used < g_max_free_nodes) {
                    best_fit->current_size = total_size + (remaining - aligned_new_size);

                    char* new_block_addr = reinterpret_cast<char*>(best_fit) + best_fit->current_size;

                    assert((reinterpret_cast<uintptr_t>(new_block_addr) & (ALIGNMENT - 1)) == 0);

                    block_t* new_block = ::new (new_block_addr) block_t(aligned_new_size, best_fit->current_size, true);

                    region_t* region = findRegionForBlock(best_fit);
                    if (region) {
                        block_t* next = getNextBlock(new_block, region);
                        if (next) {
                            next->prev_size = new_block->current_size;
                        }
                    }

                    free_node_t* new_node = allocateFreeNode();
                    if (new_node) {
                        new_node->header      = new_block;
                        size_t user_size      = new_block->current_size - sizeof(block_t);
                        size_t new_list_index = getCoalesceListIndex(user_size);
                        addToFreeList(new_node, new_list_index);
                    }
                }
            }
        }
    }

#if ALLOCATOR_DEBUG
    stats.coalesce_alloc_count++;
    stats.total_allocations++;
    stats.current_allocated += size;
    stats.peak_allocated = std::max(stats.peak_allocated, stats.current_allocated);
#endif

    void* result = getPointerFromBlock(best_fit);
    assert((reinterpret_cast<uintptr_t>(result) & (ALIGNMENT - 1)) == 0);
    return result;
}

void freeCoalesce(void* ptr, Statistics& stats) noexcept
{
    if (!ptr) {
        return;
    }

    block_t* block = getBlockFromPointer(ptr);
    if (!block) {
        return;
    }

    size_t user_size = block->current_size - sizeof(block_t);
    block->is_free   = true;

    region_t* region = findRegionForBlock(block);
    if (!region) {
        return;
    }

    block_t* prev = getPrevBlock(block);
    if (prev && prev->is_free && isBlockInRegion(prev, region)) {
        if (prev->free_node) {
            removeFromFreeList(prev->free_node);
        }
        mergeBlocks(prev, block);
        block = prev;
    }

    block_t* next = getNextBlock(block, region);
    if (next && next->is_free) {
        if (next->free_node) {
            removeFromFreeList(next->free_node);
        }
        mergeBlocks(block, next);
    }

    free_node_t* new_node = allocateFreeNode();
    if (new_node) {
        new_node->header  = block;
        size_t user_size  = block->current_size - sizeof(block_t);
        size_t list_index = getCoalesceListIndex(user_size);
        addToFreeList(new_node, list_index);
    }

#if ALLOCATOR_DEBUG
    stats.total_frees++;
    stats.current_allocated -= user_size;
#endif
}

void initFSA(MemoryAllocator::FSAPool& pool, size_t block_size, char* memory, size_t mem_size) noexcept
{
    pool.block_size  = block_size;
    pool.memory_pool = memory;
    pool.pool_size   = mem_size;
    pool.used_blocks = 0;
    pool.free_list   = nullptr;

    assert((reinterpret_cast<uintptr_t>(memory) & (ALIGNMENT - 1)) == 0);

    size_t block_count = mem_size / block_size;

    for (size_t i = 0; i < block_count; ++i) {
        free_list_t* block = reinterpret_cast<free_list_t*>(memory + i * block_size);

        assert((reinterpret_cast<uintptr_t>(block) & (ALIGNMENT - 1)) == 0);

        block->next    = pool.free_list;
        pool.free_list = block;
    }
}

void* allocFSA(MemoryAllocator::FSAPool& pool, Statistics& stats) noexcept
{
    if (!pool.free_list) {
        return nullptr;
    }

    free_list_t* block = pool.free_list;
    pool.free_list     = block->next;
    pool.used_blocks++;

#if ALLOCATOR_DEBUG
    stats.fsa_alloc_count++;
    stats.total_allocations++;
    stats.current_allocated += pool.block_size;
    stats.peak_allocated = std::max(stats.peak_allocated, stats.current_allocated);
#endif

    return block;
}

void freeFSA(void* ptr, MemoryAllocator::FSAPool& pool, Statistics& stats) noexcept
{
    free_list_t* block = static_cast<free_list_t*>(ptr);
    block->next        = pool.free_list;
    pool.free_list     = block;
    pool.used_blocks--;

#if ALLOCATOR_DEBUG
    stats.total_frees++;
    stats.current_allocated -= pool.block_size;
#endif
}

inline bool isInFSAArena(void* ptr, char* fsa_arena_start, char* fsa_arena_end) noexcept
{
    assert(fsa_arena_start < fsa_arena_end && "fsa area start > sfa area end!!!");
    return ptr >= fsa_arena_start && ptr < fsa_arena_end;
}

MemoryAllocator::MemoryAllocator()
    : coalesce_alloc_(REGION_SIZE)
{
}

MemoryAllocator::~MemoryAllocator()
{
    destroy();
}

void MemoryAllocator::init()
{
    if (is_initialized_)
        return;

    g_virtual_memory = static_cast<char*>(mmap(nullptr, TOTAL_VIRTUAL_MEMORY, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));

    if (g_virtual_memory == MAP_FAILED) {
        std::cerr << "Failed to allocate virtual memory" << std::endl;
        perror("mmap");
        return;
    }

#if ALLOCATOR_DEBUG
    stats_.mmap_count++;
#endif

    mprotect(g_virtual_memory, PAGE_SIZE, PROT_NONE);
    mprotect(g_virtual_memory + TOTAL_VIRTUAL_MEMORY - PAGE_SIZE, PAGE_SIZE, PROT_NONE);

    char* usable_memory = g_virtual_memory + PAGE_SIZE;
    size_t usable_size  = TOTAL_VIRTUAL_MEMORY - PAGE_SIZE * 2;
    size_t offset       = 0;

    auto advanceAligned = [&](size_t size) -> char* {
        offset       = alignSize(offset);
        char* result = usable_memory + offset;
        offset += size;
        return result;
    };

    g_regions = reinterpret_cast<region_t*>(advanceAligned(MAX_REGIONS * sizeof(region_t)));
    for (size_t i = 0; i < MAX_REGIONS; ++i) {
        g_regions[i].start       = nullptr;
        g_regions[i].end         = nullptr;
        g_regions[i].is_used     = false;
        g_regions[i].region_type = RegionType::SMALL;
    }

    g_free_lists = reinterpret_cast<free_node_t**>(advanceAligned(MemoryAllocator::COALESCE_LISTS_COUNT * sizeof(free_node_t*)));
    for (size_t i = 0; i < MemoryAllocator::COALESCE_LISTS_COUNT; ++i) {
        g_free_lists[i] = nullptr;
    }

    if (offset >= usable_size) [[unlikely]] {
        std::cerr << "Not enough space for metadata" << std::endl;
        munmap(g_virtual_memory, TOTAL_VIRTUAL_MEMORY);
        return;
    }

    size_t nodes_memory = usable_size / 10;
    g_max_free_nodes    = nodes_memory / sizeof(free_node_t);
    if (g_max_free_nodes < 10000) {
        g_max_free_nodes = 10000;
    }

    g_free_nodes_pool = reinterpret_cast<free_node_t*>(advanceAligned(g_max_free_nodes * sizeof(free_node_t)));

    for (size_t i = 0; i < g_max_free_nodes; ++i) {
        ::new (&g_free_nodes_pool[i]) free_node_t();
    }
    g_free_nodes_used = 0;

    size_t fsa_arena_size = alignToPage(FSA_ARENA_SIZE);
    offset                = alignSize(offset);

    if (offset + fsa_arena_size > usable_size) [[unlikely]] {
        std::cerr << "Not enough space for FSA arena" << std::endl;
        munmap(g_virtual_memory, TOTAL_VIRTUAL_MEMORY);
        g_virtual_memory = nullptr;
        return;
    }

    fsa_arena_start_ = usable_memory + offset;
    fsa_arena_end_   = fsa_arena_start_ + fsa_arena_size;
    offset += fsa_arena_size;

    size_t fsa_memory_per_pool = fsa_arena_size / FSA_SIZES_COUNT;
    fsa_memory_per_pool        = alignSize(fsa_memory_per_pool);

    char* current_pool_start = fsa_arena_start_;
    for (size_t i = 0; i < FSA_SIZES_COUNT; ++i) {
        uintptr_t addr = reinterpret_cast<uintptr_t>(current_pool_start);
        if (addr & (ALIGNMENT - 1)) {
            addr               = alignSize(addr);
            current_pool_start = reinterpret_cast<char*>(addr);
        }

        size_t actual_pool_size = fsa_memory_per_pool;
        if (current_pool_start + actual_pool_size > fsa_arena_end_) {
            actual_pool_size = fsa_arena_end_ - current_pool_start;
        }

        initFSA(fsa_pools_[i], FSA_SIZES[i], current_pool_start, actual_pool_size);
        current_pool_start += actual_pool_size;
    }

    g_current_offset = alignSize(offset);

    for (size_t i = 0; i < REGION_COUNT_BY_TYPE; ++i) {
        region_t* region = allocateRegionByType(static_cast<RegionType>(i));
        if (region) {
            initializeRegion(region);
        }
    }

    is_initialized_ = true;
}

void MemoryAllocator::destroy()
{
    if (!is_initialized_ || !g_virtual_memory)
        return;

    munmap(g_virtual_memory, TOTAL_VIRTUAL_MEMORY);

    g_virtual_memory  = nullptr;
    g_regions         = nullptr;
    g_free_nodes_pool = nullptr;
    g_free_lists      = nullptr;
    g_free_nodes_used = 0;
    g_current_offset  = 0;
    g_max_free_nodes  = 0;

    fsa_arena_start_ = nullptr;
    fsa_arena_end_   = nullptr;

    for (size_t i = 0; i < FSA_SIZES_COUNT; ++i) {
        fsa_pools_[i].memory_pool = nullptr;
        fsa_pools_[i].free_list   = nullptr;
        fsa_pools_[i].used_blocks = 0;
    }

    is_initialized_ = false;

#if ALLOCATOR_DEBUG
    stats_.munmap_count++;
#endif
}

void* MemoryAllocator::alloc(size_t size)
{
    assert(is_initialized_ && "allocator need to be initilized");

    if (size == 0) {
        return nullptr;
    }

    size_t aligned_size = alignSize(size);
    void* result        = nullptr;

    if (aligned_size < LARGE_ALLOC_THRESHOLD) [[likely]] {
        size_t size_class = getFSASizeClass(aligned_size);
        if (size_class < FSA_SIZES_COUNT) {
            result = allocFSA(fsa_pools_[size_class], stats_);
            if (!result) {
                result = allocateFromCoalesce(aligned_size, stats_);
            }
        } else {
            result = allocateFromCoalesce(aligned_size, stats_);
        }
    } else {
        result = ::malloc(aligned_size);
#if ALLOCATOR_DEBUG
        if (result) {
            stats_.large_alloc_count++;
            stats_.total_allocations++;
            stats_.current_allocated += aligned_size;
            stats_.peak_allocated     = std::max(stats_.peak_allocated, stats_.current_allocated);
            large_allocs_map_[result] = aligned_size;
        }
#endif
    }

    return result;
}

void MemoryAllocator::free(void* p)
{
    assert(is_initialized_ && "allocator need to be initilized");
    if (!p)
        return;

    if (isInFSAArena(p, fsa_arena_start_, fsa_arena_end_)) {
        for (size_t i = 0; i < FSA_SIZES_COUNT; ++i) {
            char* pool_start = fsa_pools_[i].memory_pool;
            char* pool_end   = pool_start + fsa_pools_[i].pool_size;
            if (p >= pool_start && p < pool_end) {
                freeFSA(p, fsa_pools_[i], stats_);
                return;
            }
        }
    } else if (isPointerInCoalesceRegion(p)) {
        freeCoalesce(p, stats_);
    } else {
#if ALLOCATOR_DEBUG
        if (auto it = large_allocs_map_.find(p); it != large_allocs_map_.end()) {
            stats_.large_alloc_count--;
            stats_.total_frees++;
            stats_.current_allocated -= it->second;
            large_allocs_map_.erase(it);
        }
#endif
        ::free(p);
    }
}

#if ALLOCATOR_DEBUG
void MemoryAllocator::dumpStat() const
{
    if (!is_initialized_) {
        std::cout << "Allocator not initialized" << std::endl;
        return;
    }

    std::cout << "=== Memory Allocator Statistics ===\n";
    std::cout << "Total allocations: " << stats_.total_allocations << "\n";
    std::cout << "Total frees: " << stats_.total_frees << "\n";
    std::cout << "Current allocated: " << stats_.current_allocated << " bytes\n";
    std::cout << "Peak allocated: " << stats_.peak_allocated << " bytes\n";
    std::cout << "FSA allocations: " << stats_.fsa_alloc_count << "\n";
    std::cout << "Coalesce allocations: " << stats_.coalesce_alloc_count << "\n";
    std::cout << "Large allocations: " << stats_.large_alloc_count << "\n";
    std::cout << "mmap calls: " << stats_.mmap_count << "\n";
    std::cout << "munmap calls: " << stats_.munmap_count << "\n";

    size_t used_regions   = 0;
    size_t small_regions  = 0;
    size_t medium_regions = 0;
    size_t large_regions  = 0;

    if (g_regions) {
        for (size_t i = 0; i < MAX_REGIONS; ++i) {
            if (g_regions[i].is_used) {
                used_regions++;
                switch (g_regions[i].region_type) {
                    case RegionType::SMALL:
                        small_regions++;
                        break;
                    case RegionType::MEDIUM:
                        medium_regions++;
                        break;
                    case RegionType::LARGE:
                        large_regions++;
                        break;
                }
            }
        }
    }

    std::cout << "\nRegion Usage:\n";
    std::cout << "  Total used: " << used_regions << "/" << MAX_REGIONS << "\n";
    std::cout << "  Small regions (<=10KB): " << small_regions << "\n";
    std::cout << "  Medium regions (<=1MB): " << medium_regions << "\n";
    std::cout << "  Large regions (<=10MB): " << large_regions << "\n";

    std::cout << "\nFSA Pool Usage:\n";
    for (size_t i = 0; i < FSA_SIZES_COUNT; ++i) {
        size_t total_blocks = fsa_pools_[i].pool_size / fsa_pools_[i].block_size;
        double usage        = static_cast<double>(fsa_pools_[i].used_blocks) / total_blocks * 100.0;
        std::cout << "  Size " << fsa_pools_[i].block_size << " bytes: " << fsa_pools_[i].used_blocks << "/" << total_blocks << " blocks (" << usage << "%)\n";
    }

    std::cout << "\nCoalesce Free Lists:\n";
    if (g_free_lists) {
        static const char* list_names[] = {"Small (<=10KB)", "Medium (<=1MB)", "Large (<=10MB)"};
        for (size_t i = 0; i < MemoryAllocator::COALESCE_LISTS_COUNT; ++i) {
            size_t count         = 0;
            free_node_t* current = g_free_lists[i];
            while (current) {
                count++;
                current = current->next;
            }
            std::cout << "  " << list_names[i] << ": " << count << " free blocks\n";
        }
    }

    std::cout << "\nFree nodes: " << g_free_nodes_used << "/" << g_max_free_nodes << " used (" << (g_free_nodes_used * 100.0 / g_max_free_nodes) << "%)\n";

    std::cout << std::endl;
}

void MemoryAllocator::dumpBlocks() const
{
    if (!is_initialized_) {
        std::cout << "Allocator not initialized" << std::endl;
        return;
    }

    std::cout << "=== Coalesce Allocator Blocks ===\n";

    if (!g_regions) {
        std::cout << "No regions allocated\n";
        return;
    }

    for (size_t i = 0; i < MAX_REGIONS; ++i) {
        if (g_regions[i].is_used) {
            const char* type_str = "";
            switch (g_regions[i].region_type) {
                case RegionType::SMALL:
                    type_str = "SMALL";
                    break;
                case RegionType::MEDIUM:
                    type_str = "MEDIUM";
                    break;
                case RegionType::LARGE:
                    type_str = "LARGE";
                    break;
            }

            std::cout << "Region " << i << " [" << type_str << "] (" << static_cast<void*>(g_regions[i].start) << " - " << static_cast<void*>(g_regions[i].end)
                      << "):\n";

            char* current    = g_regions[i].start;
            size_t block_num = 0;

            while (current < g_regions[i].end) {
                block_t* block = reinterpret_cast<block_t*>(current);
                std::cout << "  Block " << block_num++ << ": addr=" << static_cast<void*>(block) << ", size=" << block->current_size
                          << ", free=" << (block->is_free ? "yes" : "no") << ", prev_size=" << block->prev_size << "\n";

                current += block->current_size;
                if (block->current_size == 0)
                    break;
            }
        }
    }

    std::cout << std::endl;
}
#endif
} // namespace jd::memory
