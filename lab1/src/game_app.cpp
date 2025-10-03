#include "game_app.hpp"

#include <charconv>
#include <cstring>
#include <exception>
#include <format>
#include <iostream>
#include <limits>
#include <stdio.h>
#include <string_view>

#include "file/backend.hpp"
#include "game_event_generator_interface.hpp"
#include "game_save_manager.hpp"
#include "path.h"

using std::literals::string_view_literals::operator""sv;

namespace
{
// clang-format off
static constexpr std::string_view the_seneschal_says =
"Житель ест бушелей:           {}\n"
"Житель обрабатывает акров:    {}\n"
"Коэффициент бушель <-> зeрно: 1 <-> 2\n\n"
"Мой повелитель, соизволь поведать тебе\n"
"     в году {} твоего высочайшего правления\n"
"     {} человек умерли с голоду, и {} человек прибыли в наш великий город;\n"
"     Чума {};\n"
"     Мы собрали {} бушелей пшеницы, по {} бушеля с акра;\n"
"     Крысы истребили {} бушеля пшеницы, оставив {} бушеля в амбарах;\n"
"     Город сейчас занимает {} акров, в нем проживает {} граждан;\n"
"     1 акр земли стоит сейчас {} бушель."sv;

static constexpr std::string_view the_seneschal_ask = "Что пожелаешь, повелитель?"sv;
static constexpr std::string_view replica_many_death = "Повелитель! Слишком много народа умерло голодной смертью, поэтому оставшиеся решили поживиться вами!"sv;
static constexpr std::string_view replica_lands = "Сколько акров земли повелеваешь купить? "sv;
static constexpr std::string_view replica_wheat_to_eat = "Сколько бушелей пшеницы повелеваешь съесть? "sv;
static constexpr std::string_view replica_sow = "Сколько бушелей пшеницы повелеваешь засеять? "sv;
static constexpr std::string_view replica_fail = "О, повелитель, пощади нас! У нас только {} человек, {} бушелей пшеницы и {} акров земли!"sv;
static constexpr std::string_view replica_what = "Прошу прощения, повелитель, что вы сказали? "sv;

static constexpr float SOW_FACTOR             = 0.5f;
static constexpr float END_GAME_DEATHS_COEFF  = 0.45f;
static constexpr int32_t WHEAT_PERSON_EAT     = 20;
static constexpr int32_t ACRES_PERSON_WORK    = 10;
static constexpr int32_t MAX_ROUNDS           = 10;
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

void clearConsole(std::string_view str = "")
{
    static constexpr const char* CSI = "\033[";
    printf("%sH%s2J%s", CSI, CSI, str.data());
}

bool readValues(int32_t& buy_land, int32_t& wheat_to_eat, int32_t& to_sow) noexcept
{
    auto readNumber = [](std::string_view prompt, int32_t& result) -> bool {
        std::string input;
        const int32_t max_value = std::numeric_limits<int32_t>::max();
        while (true) {
            std::cout << prompt;
            std::cout.flush();
            if (!std::getline(std::cin, input)) {
                throw std::runtime_error{"input/output error!"};
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

bool mayContinue(std::string_view prompt)
{
    clearConsole();

    while (true) {
        std::cout << prompt;

        char ch;
        if (std::cin >> ch) {
            ch = std::tolower(static_cast<unsigned char>(ch));

            // ignore other syms
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

            if (ch == 'y')
                return true;
            if (ch == 'n')
                return false;
        }
        std::cout << std::endl;
    }
}

} // namespace

namespace jd
{
GameApplication::GameApplication(std::unique_ptr<IGameEventGenerator> generator)
    : town_ctx_{TOWN_CONTEXT_DEFAULT}
    , generator_{std::move(generator)}
    , saves_manager_{std::make_shared<file::Backend>(ROOT_DIR "/saves/save.bin")}
{
}

void GameApplication::run()
{
    if (load() && town_ctx_.year != 0) {
        game_status_ = mayContinue("Хотите продолжить игру(y/n)? "sv) ? Status::INGAME : Status::PREPARE;
    } else {
        game_status_ = Status::PREPARE;
    }

    // Main game Loop
    while (true) {
        switch (game_status_) {
            case Status::PREPARE:
                town_ctx_    = TownContext{TOWN_CONTEXT_DEFAULT};
                round_ctx_   = generator_->generateInitRoundEvent();
                game_status_ = Status::INGAME;
                [[fallthrough]];
            case Status::INGAME:
                render();
                update();
                break;
            case Status::FINAL:
                std::memset(&town_ctx_, 0, sizeof(TownContext));
                std::memset(&round_ctx_, 0, sizeof(RoundContext));
                std::cin.get();
                [[fallthrough]];
            case Status::EXIT:
                save();
                clearConsole();
                return;
            case Status::LOSE:
                clearConsole(replica_many_death);
                game_status_ = Status::FINAL;
                break;
            case Status::WIN:
                final();
                game_status_ = Status::FINAL;
                break;
            default:
                return;
        }
    }
}

void GameApplication::update()
{
    int32_t buy_land{};
    int32_t wheat_to_eat{};
    int32_t land_to_sow{};
    int64_t total_wheat_req{};

    while (true) {
        if (!readValues(buy_land, wheat_to_eat, land_to_sow)) {
            game_status_ = Status::EXIT;
            return;
        }

        total_wheat_req = buy_land * round_ctx_.land_price + wheat_to_eat + static_cast<int32_t>(land_to_sow * SOW_FACTOR);

        if (total_wheat_req >= std::numeric_limits<int32_t>::max() || total_wheat_req > town_ctx_.wheat_bushels ||
            wheat_to_eat > (WHEAT_PERSON_EAT * town_ctx_.population) || land_to_sow > (town_ctx_.land_acres + buy_land) ||
            land_to_sow > (ACRES_PERSON_WORK * town_ctx_.population)) {
            std::cout << std::format(replica_fail, town_ctx_.population, town_ctx_.wheat_bushels, town_ctx_.land_acres) << std::endl;
            continue;
        }

        break;
    }

    if (int32_t need_wheat_to_eat = town_ctx_.population * WHEAT_PERSON_EAT; need_wheat_to_eat > wheat_to_eat) {
        town_ctx_.fear_and_hunder_deaths_this_year = (need_wheat_to_eat - wheat_to_eat) / WHEAT_PERSON_EAT;
    }

    const float deaths_stat = static_cast<float>(town_ctx_.fear_and_hunder_deaths_this_year) / static_cast<float>(town_ctx_.population);
    if (deaths_stat > END_GAME_DEATHS_COEFF) {
        game_status_ = Status::LOSE;
        return;
    }

    town_ctx_.wheat_bushels -= total_wheat_req;
    town_ctx_.fear_and_hunder_deaths_mean += deaths_stat;
    town_ctx_.population -= town_ctx_.fear_and_hunder_deaths_this_year;
    town_ctx_.land_acres += buy_land;
    town_ctx_.wheat_harvested_this_year = land_to_sow * round_ctx_.wheat_yield;
    town_ctx_.wheat_bushels += town_ctx_.wheat_harvested_this_year;
    town_ctx_.wheat_yield_per_acre = round_ctx_.wheat_yield;

    round_ctx_ = generator_->generateRoundEvent(town_ctx_);

    town_ctx_.wheat_bushels -= round_ctx_.rats_damage;
    town_ctx_.population = town_ctx_.population + round_ctx_.new_citizens - round_ctx_.plague_deaths;

    ++town_ctx_.year;

    if (town_ctx_.year > MAX_ROUNDS) {
        game_status_ = Status::WIN;
    }
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

    const std::string_view plague_info = round_ctx_.plague_occurred ? "уничтожила половину населения"sv : "обошла нас стороной"sv;

    std::cout << std::format(
                     the_seneschal_says,
                     WHEAT_PERSON_EAT,
                     ACRES_PERSON_WORK,
                     town_ctx_.year,
                     town_ctx_.fear_and_hunder_deaths_this_year,
                     round_ctx_.new_citizens,
                     plague_info,
                     town_ctx_.wheat_harvested_this_year,
                     town_ctx_.wheat_yield_per_acre,
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
    clearConsole();

    static constinit const ValidationRule rules[] = {
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

    const float P    = town_ctx_.fear_and_hunder_deaths_mean / static_cast<float>(town_ctx_.year);
    const uint32_t L = town_ctx_.land_acres / town_ctx_.population;

    for (const ValidationRule& rule : rules) {
        if (rule.predicate(P, L)) {
            std::cout << rule.comment << std::endl;
            break;
        }
    }
}

} // namespace jd
