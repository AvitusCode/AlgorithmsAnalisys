#pragma once
#include "round_context.hpp"

namespace jd
{

struct TownContext;

class IGameEventGenerator
{
public:
    virtual ~IGameEventGenerator() = default;

    [[nodiscard]] virtual RoundContext generateInitRoundEvent() const                        = 0;
    [[nodiscard]] virtual RoundContext generateRoundEvent(const TownContext& town_ctx) const = 0;
};
} // namespace jd
