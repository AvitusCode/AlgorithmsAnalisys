#include <cstddef>

#ifndef NDEBUG
#define ALLOCATOR_DEBUG 1
#include "allocator_stats.hpp"
#include <unordered_map>
#else
#define ALLOCATOR_DEBUG 0
#endif

namespace jd::memory
{
class MemoryAllocator final
{
public:
    ~MemoryAllocator();

    MemoryAllocator(const MemoryAllocator&)            = delete;
    MemoryAllocator& operator=(const MemoryAllocator&) = delete;
    MemoryAllocator(MemoryAllocator&&)                 = delete;
    MemoryAllocator& operator=(MemoryAllocator&&)      = delete;

    static MemoryAllocator& allocator() noexcept
    {
        static MemoryAllocator allocator{};
        return allocator;
    }

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
private:
    MemoryAllocator() = default;
    bool is_initialized_{false};
#if ALLOCATOR_DEBUG
    Statistics stats_;
    std::unordered_map<void*, size_t> large_allocs_map_;
#endif
};
} // namespace jd::memory
