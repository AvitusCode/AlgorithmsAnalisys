#pragma once

#include <memory>

#include "game_round.hpp"
#include "town_context.hpp"

namespace jd
{
namespace file
{
class IBackend;
}
class GameApplication
{
public:
    GameApplication(std::unique_ptr<IGameEventGenerator> generator);
    ~GameApplication();

    void run();

private:
    void update();
    bool save();
    bool load();
    void render();

    inline static constexpr const int32_t MAX_ROUNDS = 10;
    std::shared_ptr<file::IBackend> town_ctx_backend_;
    std::shared_ptr<file::IBackend> round_ctx_backend_;
    GameRound round_;
    TownContext town_ctx_{};
    RoundContext round_ctx_{};
    int32_t died_with_fear_and_hunger_{};
};
} // namespace jd
