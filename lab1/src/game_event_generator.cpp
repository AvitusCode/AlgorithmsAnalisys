#include "game_event_generator.hpp"

#include <algorithm>

#include "town_context.hpp"

#include <yaml-cpp/yaml.h>

namespace jd
{

GameEventGenerator::GameEventGenerator(const std::string& config_file)
    : rand_(std::random_device{}())
{
    try {
        YAML::Node config  = YAML::LoadFile(config_file);
        const auto& events = config["random_events"];

        land_price_min_ = events["land_price"]["min"].as<int32_t>();
        land_price_max_ = events["land_price"]["max"].as<int32_t>();

        wheat_yield_min_ = events["wheat_yield"]["min"].as<int32_t>();
        wheat_yield_max_ = events["wheat_yield"]["max"].as<int32_t>();

        rats_min_percent_ = events["rats_activity"]["min_percent"].as<int32_t>();
        rats_max_percent_ = events["rats_activity"]["max_percent"].as<int32_t>();

        migration_min_count_ = events["migration"]["min"].as<int32_t>();
        migration_max_count_ = events["migration"]["max"].as<int32_t>();

        plague_chance_ = events["plague"]["chance"].as<float>();

    } catch (const std::exception& e) {
        // defaults
        land_price_min_      = 17;
        land_price_max_      = 26;
        wheat_yield_min_     = 1;
        wheat_yield_max_     = 6;
        rats_min_percent_    = 0;
        rats_max_percent_    = 10;
        migration_min_count_ = 0;
        migration_max_count_ = 50;
        plague_chance_       = 0.15;
    }
}

RoundContext GameEventGenerator::generateInitRoundEvent() const
{
    return {
        .land_price      = generateLandPrice(),
        .wheat_yield     = generateWheatYield(),
        .rats_damage     = 0,
        .new_citizens    = 0,
        .plague_deaths   = 0,
        .plague_occurred = false};
}

RoundContext GameEventGenerator::generateRoundEvent(const TownContext& town_ctx) const
{
    RoundContext results;

    results.land_price      = generateLandPrice();
    results.wheat_yield     = generateWheatYield();
    results.rats_damage     = generateRatsActivity(town_ctx.wheat_bushels);
    results.plague_occurred = checkPlague();
    results.plague_deaths   = !results.plague_occurred ? 0 : town_ctx.population / 2;
    results.new_citizens    = generateMigration(results.plague_deaths, town_ctx.wheat_bushels, results.wheat_yield);

    return results;
}

int32_t GameEventGenerator::generateLandPrice() const
{
    return randomInt(land_price_min_, land_price_max_);
}

int32_t GameEventGenerator::generateWheatYield() const
{
    return randomInt(wheat_yield_min_, wheat_yield_max_);
}

int32_t GameEventGenerator::generateRatsActivity(int32_t total_wheat) const
{
    int32_t percent = randomInt(rats_min_percent_, rats_max_percent_);
    return calculatePercentage(total_wheat, percent);
}

int32_t GameEventGenerator::generateMigration(int32_t current_death, int32_t total_wheat, int32_t wheat_yield) const
{
    int32_t migration = current_death / 2 + (5 - wheat_yield) * total_wheat / 600 + 1;
    return std::clamp(migration, migration_min_count_, migration_max_count_);
}

bool GameEventGenerator::checkPlague() const
{
    return randomFloating(0.0f, 1.0f) < plague_chance_;
}

int32_t GameEventGenerator::randomInt(int32_t min, int32_t max) const
{
    std::uniform_int_distribution<int32_t> dist{min, max};
    return dist(rand_);
}

float GameEventGenerator::randomFloating(float min, float max) const
{
    std::uniform_real_distribution<float> dist{min, max};
    return dist(rand_);
}

int32_t GameEventGenerator::calculatePercentage(int32_t value, int32_t percent) const
{
    return static_cast<int32_t>(value * percent / 100.0f);
}

} // namespace jd
