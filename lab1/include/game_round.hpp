#pragma once
#include "game_event_generator_interface.hpp"
#include <memory>
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

struct TownContext;

class GameRound
{
public:
    GameRound(std::unique_ptr<IGameEventGenerator> generator);

    [[nodiscard]] RoundContext generateInitRoundEvent() const;
    [[nodiscard]] RoundContext generateRoundEvent(const TownContext& town_ctx) const;

private:
    std::unique_ptr<IGameEventGenerator> generator_;
};

} // namespace jd
