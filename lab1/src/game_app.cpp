#include "game_app.hpp"

#include <charconv>
#include <format>
#include <iostream>
#include <limits>
#include <stdio.h>

#include "file/backend.hpp"
#include "game_save_manager.hpp"
#include "path.h"

// clang-format off
static constexpr const char* the_seneschal_says =
"Мой повелитель, соизволь поведать тебе\n"
"     в году {} твоего высочайшего правления\n"
"     {} человек умерли с голоду, и {} человек прибыли в наш великий город;\n"
"     Чума {};\n"
"     Мы собрали {} бушелей пшеницы, по {} бушеля с акра;\n"
"     Крысы истребили {} бушеля пшеницы, оставив {} бушеля в амбарах;\n"
"     Город сейчас занимает {} акров;\n"
"     1 акр земли стоит сейчас {} бушель.";

static constexpr const char* the_seneschal_ask = "Что пожелаешь, повелитель?";
static constexpr const char* replica_lands = "Сколько акров земли повелеваешь купить? ";
static constexpr const char* replica_wheat_to_eat = "Сколько бушелей пшеницы повелеваешь съесть? ";
static constexpr const char* replica_sow = "Сколько бушелей пшеницы повелеваешь засеять? ";
static constexpr const char* replica_fail = "О, повелитель, пощади нас! У нас только {} человек, {} бушелей пшеницы и {} акров земли!";
static constexpr const char* replica_what = "Прошу прощения, повелитель, что вы сказали? ";
// clang-format on

static void clearConsole()
{
    static const char* CSI = "\033[";
    printf("%sH%s2J", CSI, CSI);
}

static bool readValues(int32_t& buy_land, int32_t& wheat_to_eat, int32_t& to_sow) noexcept
{
    auto readNumber = [](const char* prompt, int32_t& result) -> bool {
        std::string input;
        const int32_t max_value = std::numeric_limits<int32_t>::max();
        while (true) {
            std::cout << prompt;
            std::cout.flush();
            if (!std::getline(std::cin, input)) {
                std::cerr << "Critical error, my Lord!" << std::endl;
                return false;
            }

            if (input == "q" or input == "Q") {
                return false;
            }

            size_t start = input.find_first_not_of(" \t");
            if (start == std::string::npos) {
                std::cout << replica_what << " ";
                std::cout.flush();
                continue;
            }

            int32_t value{};
            auto [ptr, ec] = std::from_chars(input.data() + start, input.data() + input.length(), value);

            if (ec != std::errc() || ptr != input.data() + input.length()) {
                std::cout << replica_what;
                std::cout.flush();
                continue;
            }

            if (value < 0) {
                std::cout << replica_what;
                std::cout.flush();
                continue;
            }

            if (value > max_value) {
                std::cout << replica_what;
                std::cout.flush();
                continue;
            }

            result = value;
            return true;
        }
    };

    bool res{false};

    try {
        res = readNumber(replica_lands, buy_land) && readNumber(replica_wheat_to_eat, wheat_to_eat) && readNumber(replica_sow, to_sow);
    } catch (const std::exception&) {
        res = false;
    }

    return res;
}

namespace jd
{
GameApplication::GameApplication(std::unique_ptr<IGameEventGenerator> generator)
    : town_ctx_backend_{std::make_shared<file::Backend>(ROOT_DIR "/saves/town_context.bin")}
    , round_ctx_backend_{std::make_shared<file::Backend>(ROOT_DIR "/saves/round_ctx.bin")}
    , round_{std::move(generator)}
    , town_ctx_{TownContext{.population = 100, .land_acres = 1000, .wheat_bushels = 2800, .year = 1}}
{
}

GameApplication::~GameApplication()
{
    save();
}

void GameApplication::run()
{
    bool is_failed{false};

    if (!load() || town_ctx_.year == 0) {
        town_ctx_  = TownContext{.population = 100, .land_acres = 1000, .wheat_bushels = 2800, .year = 1};
        round_ctx_ = round_.generateInitRoundEvent();
    }

    while (town_ctx_.year <= MAX_ROUNDS && !is_failed) {
        clearConsole();
        render();

        // Put the information about the resources
        while (true) {
            int32_t buy_land{};
            int32_t wheat_to_eat{};
            int32_t to_sow{};

            if (!readValues(buy_land, wheat_to_eat, to_sow)) {
                save();
                return;
            }

            int64_t total_wheat_req = buy_land * round_ctx_.land_price + wheat_to_eat + to_sow / 2;

            if (total_wheat_req > town_ctx_.wheat_bushels || wheat_to_eat > (20 * town_ctx_.population) || to_sow > (town_ctx_.land_acres + buy_land) ||
                to_sow > 10 * town_ctx_.population) {
                std::cout << std::format(replica_fail, town_ctx_.population, town_ctx_.wheat_bushels, town_ctx_.land_acres) << std::endl;
                continue;
            }

            if (int32_t need_wheat_to_eat = town_ctx_.population * 20; need_wheat_to_eat > wheat_to_eat) {
                died_with_fear_and_hunger_ = (need_wheat_to_eat - wheat_to_eat) / 20;
            }

            if (static_cast<float>(died_with_fear_and_hunger_) / static_cast<float>(town_ctx_.population) > 0.45f) {
                is_failed = true;
                break;
            }

            town_ctx_.wheat_bushels -= total_wheat_req;
            town_ctx_.population -= died_with_fear_and_hunger_;
            town_ctx_.land_acres += buy_land;

            // A new crop with rats activity
            town_ctx_.wheat_bushels += to_sow * round_ctx_.wheat_yield - round_ctx_.rats_damage;
            round_ctx_ = round_.generateRoundEvent(died_with_fear_and_hunger_, town_ctx_.wheat_bushels);

            town_ctx_.population = town_ctx_.population + round_ctx_.new_citizens - round_ctx_.plague_deaths;

            ++town_ctx_.year;
            break;
        }
    }

    if (town_ctx_.year == MAX_ROUNDS) {
        // print stats and delete saves

        // zeroes saves
        town_ctx_ = TownContext{.population = 0, .land_acres = 0, .wheat_bushels = 0, .year = 0};
        save();
    }
}

void GameApplication::update() {}

bool GameApplication::save()
{
    GameSaveManger<TownContext> town_save{town_ctx_backend_};
    GameSaveManger<RoundContext> round_save{round_ctx_backend_};
    return town_save.serialize(town_ctx_) && round_save.serialize(round_ctx_);
}

bool GameApplication::load()
{
    GameSaveManger<TownContext> town_save{town_ctx_backend_};
    GameSaveManger<RoundContext> round_save{round_ctx_backend_};
    return town_save.deserialize(town_ctx_) && round_save.deserialize(round_ctx_);
}

void GameApplication::render()
{
    const char* plague_info = round_ctx_.plague_occurred ? " уничтожила половину населения" : " обошла нас стороной";
}
} // namespace jd
