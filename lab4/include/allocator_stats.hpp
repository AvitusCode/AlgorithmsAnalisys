#include <cstddef>

namespace jd::memory
{
struct Statistics {
    size_t total_allocations{};
    size_t total_frees{};
    size_t fsa_alloc_count{};
    size_t coalesce_alloc_count{};
    size_t large_alloc_count{};
    size_t current_allocated{};
    size_t peak_allocated{};
    size_t regions_count{};
};
} // namespace jd::memory
