#pragma once
#include <cstdint>
#include <type_traits>

namespace jd
{
// The structure contains values that are randomly generated at the end of each round.
struct RoundContext {
    int32_t land_price;
    int32_t wheat_yield;
    int32_t rats_damage;
    int32_t new_citizens;
    int32_t plague_deaths;
    bool plague_occurred;
};

static_assert(std::is_trivial_v<RoundContext> && std::is_standard_layout_v<RoundContext>, "RoundContext must be a POD type for serialization");
} // namespace jd
