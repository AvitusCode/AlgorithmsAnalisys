#pragma once
#include <cstdint>
#include <type_traits>

namespace jd
{
// The structure contains the current state of the city
struct TownContext {
    int32_t population;
    int32_t fear_and_hunder_deaths_this_year;
    float fear_and_hunder_deaths_mean;
    int32_t land_acres;
    int32_t wheat_bushels;
    int32_t wheat_yield_per_acre;
    int32_t wheat_harvested_this_year;
    int32_t year; // round
};

static_assert(std::is_trivial_v<TownContext> && std::is_standard_layout_v<TownContext>, "TownContext must be a POD type for serialization");

inline static constexpr TownContext TOWN_CONTEXT_DEFAULT = TownContext{
    .population                       = 100,
    .fear_and_hunder_deaths_this_year = 0,
    .fear_and_hunder_deaths_mean      = 0.0f,
    .land_acres                       = 1000,
    .wheat_bushels                    = 2800,
    .wheat_yield_per_acre             = 0,
    .wheat_harvested_this_year        = 0,
    .year                             = 1};
} // namespace jd
