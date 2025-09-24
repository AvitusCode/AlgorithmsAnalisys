#pragma once

#include <memory>

#include "game_round.hpp"
#include "game_save_manager.hpp"
#include "town_context.hpp"

namespace jd
{
class GameApplication
{
public:
    GameApplication(std::unique_ptr<IGameEventGenerator> generator);
    ~GameApplication();

    void run();

private:
    bool update();
    bool save();
    bool load();
    void render();
    void final();

    GameRound round_;
    TownContext town_ctx_;
    RoundContext round_ctx_;
    GameSavesManager<sizeof(TownContext) + sizeof(RoundContext)> saves_manager_;
};
} // namespace jd
