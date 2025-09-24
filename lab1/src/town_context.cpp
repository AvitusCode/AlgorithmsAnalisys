#include "town_context.hpp"

namespace jd
{
bool operator==(const TownContext& lhs, const TownContext& rhs) noexcept
{
    return lhs.year == rhs.year && lhs.population == rhs.population && lhs.wheat_bushels == rhs.wheat_bushels && lhs.land_acres == rhs.land_acres &&
           lhs.wheat_harvested_this_year == rhs.wheat_harvested_this_year && lhs.fear_and_hunder_deaths_mean == rhs.fear_and_hunder_deaths_mean &&
           lhs.fear_and_hunder_deaths_this_year == rhs.fear_and_hunder_deaths_this_year;
}

std::ostream& operator<<(std::ostream& os, const TownContext& town) noexcept
{
    os << "TownContext {\n";
    os << "  population = " << town.population << "\n";
    os << "  fear_and_hunder_deaths_this_year = " << town.fear_and_hunder_deaths_this_year << "\n";
    os << "  fear_and_hunder_deaths_total = " << town.fear_and_hunder_deaths_mean << "\n";
    os << "  land_acres = " << town.land_acres << "\n";
    os << "  wheat_bushels = " << town.wheat_bushels << "\n";
    os << "  wheat_harvested_this_year = " << town.wheat_harvested_this_year << "\n";
    os << "  year = " << town.year << "\n";
    os << "}";
    return os;
}
} // namespace jd
