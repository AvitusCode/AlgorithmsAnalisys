#include <iostream>
#include <locale>
#include <memory>

#include "file/backend.hpp"
#include "game_app.hpp"
#include "game_event_generator.hpp"
#include "game_save_manager.hpp"
#include "path.h"
#include "town_context.hpp"

/*
 * TODO:
 * 1) Modify tests
 * 2) Final Redactoring
 * 3) test and play it!
 * */

void test_save_manager()
{
    auto backend = std::make_unique<jd::file::Backend>(ROOT_DIR "/saves/save.bin");
    jd::GameSavesManager<sizeof(jd::TownContext)> manager{std::move(backend)};

    jd::TownContext gameState                  = jd::TOWN_CONTEXT_DEFAULT;
    gameState.fear_and_hunder_deaths_this_year = 11;
    gameState.fear_and_hunder_deaths_mean      = 0.12f;

    std::cout << "Saving game state..." << std::endl;
    auto writer = manager.writer();
    if (writer.write(gameState).commit()) {
        std::cout << "Game saved successfully!" << std::endl;
    } else {
        std::cout << "Failed to save game!" << std::endl;
    }

    std::cout << "Loading game state..." << std::endl;
    manager.reset();
    auto reader = manager.reader();
    jd::TownContext loadedState;
    reader.load().read(loadedState);
    if (reader.isLoaded()) {
        if (loadedState == gameState) {
            std::cout << "Game loaded successfully!" << std::endl;

            std::cout << loadedState << std::endl;
        } else {
            std::cout << "Failed to load game!" << std::endl;
        }
    } else {
        std::cout << "Failed to load game!" << std::endl;
    }
}

int main(void)
{
    // test_save_manager();

    auto generator = std::make_unique<jd::GameEventGenerator>(ROOT_DIR "configs/configs.yaml");
    auto gameApp   = std::make_unique<jd::GameApplication>(std::move(generator));

    try {
        std::locale::global(std::locale("en_US.UTF-8"));

        gameApp->run();
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << std::endl;
        std::cin.get();
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
