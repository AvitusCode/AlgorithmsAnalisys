#pragma once

#include <memory>

#include "game_save_manager.hpp"
#include "round_context.hpp"
#include "town_context.hpp"

namespace jd
{

class IGameEventGenerator;

class GameApplication
{
public:
    GameApplication(std::unique_ptr<IGameEventGenerator> generator);

    void run();

private:
    enum class Status {
        PREPARE,
        INGAME,
        WIN,
        LOSE,
        EXIT,
        FINAL,
    };

    bool save();
    bool load();
    void render();
    void update();
    void final();

    Status game_status_{Status::INGAME};
    TownContext town_ctx_;
    RoundContext round_ctx_;
    std::unique_ptr<IGameEventGenerator> generator_;
    GameSavesManager<sizeof(TownContext) + sizeof(RoundContext)> saves_manager_;
};
} // namespace jd
