#include "game_round.hpp"

namespace jd
{
GameRound::GameRound(std::unique_ptr<IGameEventGenerator> generator)
    : generator_(std::move(generator))
{
}

RoundContext GameRound::generateInitRoundEvent() const
{
    RoundContext results;

    results.land_price      = generator_->generateLandPrice();
    results.wheat_yield     = generator_->generateWheatYield();
    results.rats_damage     = 0;
    results.plague_occurred = false;
    results.plague_deaths   = 0;
    results.new_citizens    = 0;

    return results;
}

RoundContext GameRound::generateRoundEvent(int32_t current_population, int32_t total_wheat) const
{
    RoundContext results;

    results.land_price      = generator_->generateLandPrice();
    results.wheat_yield     = generator_->generateWheatYield();
    results.rats_damage     = generator_->generateRatsActivity(total_wheat);
    results.plague_occurred = generator_->checkPlague();
    results.plague_deaths   = results.plague_occurred ? current_population / 2 : 0;
    results.new_citizens    = generator_->generateMigration(results.plague_deaths, total_wheat, results.wheat_yield);

    return results;
}

} // namespace jd
