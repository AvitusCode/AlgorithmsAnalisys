#include "game_app.hpp"

#include <array>
#include <charconv>
#include <cstring>
#include <format>
#include <iostream>
#include <limits>
#include <stdio.h>
#include <string_view>

#include "file/backend.hpp"
#include "game_save_manager.hpp"
#include "path.h"

namespace
{
// clang-format off
static constexpr const char* the_seneschal_says =
"Мой повелитель, соизволь поведать тебе\n"
"     в году {} твоего высочайшего правления\n"
"     {} человек умерли с голоду, и {} человек прибыли в наш великий город;\n"
"     Чума {};\n"
"     Мы собрали {} бушелей пшеницы, по {} бушеля с акра;\n"
"     Крысы истребили {} бушеля пшеницы, оставив {} бушеля в амбарах;\n"
"     Город сейчас занимает {} акров, в нем проживает {} граждан;\n"
"     1 акр земли стоит сейчас {} бушель.";

static constexpr const char* the_seneschal_ask = "Что пожелаешь, повелитель?";
static constexpr const char* replica_lands = "Сколько акров земли повелеваешь купить? ";
static constexpr const char* replica_wheat_to_eat = "Сколько бушелей пшеницы повелеваешь съесть? ";
static constexpr const char* replica_sow = "Сколько бушелей пшеницы повелеваешь засеять? ";
static constexpr const char* replica_fail = "О, повелитель, пощади нас! У нас только {} человек, {} бушелей пшеницы и {} акров земли!";
static constexpr const char* replica_what = "Прошу прощения, повелитель, что вы сказали? ";
// clang-format on

struct ValidationRule {
    using predication_t = bool (*)(float, int32_t) noexcept;

    predication_t predicate;
    std::string_view comment;

    consteval ValidationRule(predication_t pred, std::string_view msg)
        : predicate{pred}
        , comment{msg}
    {
    }
};

void clearConsole(const char* str = "")
{
    static constexpr const char* CSI = "\033[";
    printf("%sH%s2J%s", CSI, CSI, str);
}

bool readValues(int32_t& buy_land, int32_t& wheat_to_eat, int32_t& to_sow) noexcept
{
    auto readNumber = [](const char* prompt, int32_t& result) -> bool {
        std::string input;
        const int32_t max_value = std::numeric_limits<int32_t>::max();
        while (true) {
            std::cout << prompt;
            std::cout.flush();
            if (!std::getline(std::cin, input)) {
                clearConsole("Critical error, my Lord!");
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
                std::cout << replica_what << "Разве числа могут быть отрицательными? ";
                std::cout.flush();
                continue;
            }

            if (value > max_value) {
                std::cout << replica_what << "Разве существуют такие большие числа? ";
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
    } catch (const std::exception& ex) {
        std::string msg = "CRITICAL ERROR: " + std::string(ex.what());
        clearConsole(msg.c_str());
        res = false;
    }

    return res;
}
} // namespace

namespace jd
{
GameApplication::GameApplication(std::unique_ptr<IGameEventGenerator> generator)
    : round_{std::move(generator)}
    , town_ctx_{TOWN_CONTEXT_DEFAULT}
    , saves_manager_{std::make_shared<file::Backend>(ROOT_DIR "/saves/save.bin")}
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
        town_ctx_  = TownContext{TOWN_CONTEXT_DEFAULT};
        round_ctx_ = round_.generateInitRoundEvent();
    }

    while (town_ctx_.year <= MAX_ROUNDS && !is_failed) {
        render();
        is_failed = update();
    }

    if (is_failed) {
        return;
    }

    if (town_ctx_.year == MAX_ROUNDS) {
        // print stats and delete saves
        final();
        // zeroes saves
        memset(&town_ctx_, 0, sizeof(TownContext));
        memset(&round_ctx_, 0, sizeof(RoundContext));
    }
}

bool GameApplication::update()
{
    static constexpr const float SOW_FACTOR             = 0.5f;
    static constexpr const float END_GAME_DEATHS_COEFF  = 0.45f;
    static constexpr const int32_t WHEAT_PERSON_EAT     = 20;
    static constexpr const int32_t WHEAT_PERSON_PRODUCE = 10;

    int32_t buy_land{};
    int32_t wheat_to_eat{};
    int32_t land_to_sow{};
    int64_t total_wheat_req{};

    while (true) {
        if (!readValues(buy_land, wheat_to_eat, land_to_sow)) {
            return false;
        }

        total_wheat_req = buy_land * round_ctx_.land_price + wheat_to_eat + static_cast<int32_t>(land_to_sow * SOW_FACTOR);

        if (total_wheat_req >= std::numeric_limits<int32_t>::max() || total_wheat_req > town_ctx_.wheat_bushels ||
            wheat_to_eat > (WHEAT_PERSON_EAT * town_ctx_.population) || land_to_sow > (town_ctx_.land_acres + buy_land) ||
            land_to_sow > (WHEAT_PERSON_PRODUCE * town_ctx_.population)) {
            std::cout << std::format(replica_fail, town_ctx_.population, town_ctx_.wheat_bushels, town_ctx_.land_acres) << std::endl;
            continue;
        }

        break;
    }

    if (int32_t need_wheat_to_eat = town_ctx_.population * WHEAT_PERSON_EAT; need_wheat_to_eat > wheat_to_eat) {
        town_ctx_.fear_and_hunder_deaths_this_year = (need_wheat_to_eat - wheat_to_eat) / WHEAT_PERSON_EAT;
    }

    if (static_cast<float>(town_ctx_.fear_and_hunder_deaths_this_year) / static_cast<float>(town_ctx_.population) > END_GAME_DEATHS_COEFF) {
        return true;
    }

    town_ctx_.wheat_bushels -= total_wheat_req;
    town_ctx_.population -= town_ctx_.fear_and_hunder_deaths_this_year;
    town_ctx_.fear_and_hunder_deaths_total += town_ctx_.fear_and_hunder_deaths_this_year;
    town_ctx_.land_acres += buy_land;
    town_ctx_.wheat_harvested_this_year = land_to_sow * round_ctx_.wheat_yield;
    town_ctx_.wheat_bushels += town_ctx_.wheat_harvested_this_year;

    round_ctx_ = round_.generateRoundEvent(town_ctx_);
    town_ctx_.wheat_bushels -= round_ctx_.rats_damage;
    town_ctx_.population = town_ctx_.population + round_ctx_.new_citizens - round_ctx_.plague_deaths;

    ++town_ctx_.year;
    return true;
}

bool GameApplication::save()
{
    try {
        saves_manager_.reset();
        auto writer = saves_manager_.writer();
        writer.write(town_ctx_).write(round_ctx_).commit();
    } catch (const std::exception& ex) {
        return false;
    }
    return true;
}

bool GameApplication::load()
{
    try {
        saves_manager_.reset();
        auto reader = saves_manager_.reader();
        reader.load().read(town_ctx_).read(round_ctx_);
    } catch (const std::exception& ex) {
        return false;
    }

    return true;
}

void GameApplication::render()
{
    clearConsole();
    const char* plague_info = round_ctx_.plague_occurred ? " уничтожила половину населения" : " обошла нас стороной";

    std::cout << std::format(
                     the_seneschal_says,
                     town_ctx_.year,
                     town_ctx_.fear_and_hunder_deaths_this_year,
                     round_ctx_.new_citizens,
                     plague_info,
                     town_ctx_.wheat_harvested_this_year,
                     round_ctx_.wheat_yield,
                     round_ctx_.rats_damage,
                     town_ctx_.wheat_bushels,
                     town_ctx_.land_acres,
                     town_ctx_.population,
                     round_ctx_.land_price)
              << std::endl;

    std::cout << the_seneschal_ask << std::endl;
}

void GameApplication::final()
{
    using namespace std::literals::string_view_literals;

    static constexpr const std::array<ValidationRule, 4> rules = {
        ValidationRule{
            +[](float P, int32_t L) noexcept { return P > 0.33f && L < 7; },
            "Из-за вашей некомпетентности в управлении, народ устроил бунт и изгнал вас из города.\nТеперь вы вынуждены влачить жалкое существование в изгнании."sv},
        ValidationRule{
            +[](float P, int32_t L) noexcept { return P > 0.1f && L < 9; },
            "Вы правили железной рукой, подобно Нерону и Ивану Грозному.\nНарод вздохнул с облегчением, и никто больше не желает видеть вас правителем."sv},
        ValidationRule{
            +[](float P, int32_t L) noexcept { return P > 0.03f && L < 10; },
            "Вы справились вполне неплохо, у вас, конечно, есть недоброжелатели,\nно многие хотели бы увидеть вас во главе города снова."sv},
        ValidationRule{+[](float, int32_t) noexcept { return true; }, "Фантастика! Карл Великий, Дизраэли и Джефферсон вместе не справились бы лучше!"sv}};

    const float P    = 0.0f;
    const uint32_t L = town_ctx_.land_acres / town_ctx_.population;

    for (const auto& rule : rules) {
        if (rule.predicate(P, L)) {
            std::cout << rule.comment << std::endl;
            break;
        }
    }

    std::cin.get();
}

} // namespace jd
