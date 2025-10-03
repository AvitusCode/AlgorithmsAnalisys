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

    [[nodiscard]] RoundContext generateInitRoundEvent() const override;
    [[nodiscard]] RoundContext generateRoundEvent(const TownContext& town_ctx) const override;

private:
    int32_t generateLandPrice() const;
    int32_t generateWheatYield() const;
    int32_t generateRatsActivity(int32_t total_wheat) const;
    int32_t generateMigration(int32_t current_death, int32_t total_wheat, int32_t wheat_yield) const;
    bool checkPlague() const;
    int32_t randomInt(int32_t min, int32_t max) const;
    float randomFloating(float min, float max) const;
    int32_t calculatePercentage(int32_t value, int32_t percent) const;

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
};
} // namespace jd
