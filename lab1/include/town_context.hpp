#pragma once
#include <cstdint>
#include <type_traits>

namespace jd
{
struct TownContext {
    int32_t population;
    int32_t land_acres;
    int32_t wheat_bushels;
    int32_t year; // round
};

static_assert(std::is_trivial_v<TownContext> && std::is_standard_layout_v<TownContext>, "TownContext must be a POD type for serialization");
} // namespace jd
