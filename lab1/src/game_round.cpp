#include "game_round.hpp"
#include "town_context.hpp"

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

RoundContext GameRound::generateRoundEvent(const TownContext& town_ctx) const
{
    RoundContext results;

    results.land_price      = generator_->generateLandPrice();
    results.wheat_yield     = generator_->generateWheatYield();
    results.rats_damage     = generator_->generateRatsActivity(town_ctx.wheat_bushels);
    results.plague_occurred = generator_->checkPlague();
    results.plague_deaths   = !results.plague_occurred ? 0 : town_ctx.population / 2;
    results.new_citizens    = generator_->generateMigration(results.plague_deaths, town_ctx.wheat_bushels, results.wheat_yield);

    return results;
}

} // namespace jd
