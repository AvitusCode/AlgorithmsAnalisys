#pragma once
#include "game_event_generator_interface.hpp"

#include <cstdint>
#include <random>
#include <string>

namespace jd
{

class GameEventGenerator : public IGameEventGenerator
{
public:
    GameEventGenerator(const std::string& config_file);

    int generateLandPrice() const override;
    int generateWheatYield() const override;
    int generateRatsActivity(int32_t total_wheat) const override;
    int generateMigration(int32_t current_death, int32_t total_wheat, int32_t wheat_yield) const override;
    bool checkPlague() const override;

private:
    mutable std::mt19937 rand_;

    int32_t land_price_min_;
    int32_t land_price_max_;

    int32_t wheat_yield_min_;
    int32_t wheat_yield_max_;

    int32_t rats_min_percent_;
    int32_t rats_max_percent_;

    int32_t migration_min_count_;
    int32_t migration_max_count_;

    float plague_chance_;

    // int32_t initial_population_;
    // int32_t initial_land_;
    // int32_t initial_wheat_;

    int32_t randomInt(int32_t min, int32_t max) const;
    float randomFloating(float min, float max) const;
    int32_t calculatePercentage(int32_t value, int32_t percent) const;
};
} // namespace jd
