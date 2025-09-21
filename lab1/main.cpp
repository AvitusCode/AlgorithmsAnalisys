#include <iostream>
#include <memory>

#include "file/backend.hpp"
#include "game_save_manager.hpp"
#include "path.h"
#include "town_context.hpp"

/*
 * TODO:
 * 1) Improve save manager to save all contexts!
 * 2) Implement render function
 * 3) Refactoring main logic (add rule models and invariant constants)
 * */

void test_save_manager()
{
    auto backend = std::make_unique<jd::file::Backend>(ROOT_DIR "/saves/save.bin");
    jd::GameSaveManger<jd::TownContext> serializer{std::move(backend)};

    jd::TownContext gameState{.population = 100, .land_acres = 1000, .wheat_bushels = 2800, .year = 1};

    std::cout << "Saving game state..." << std::endl;
    if (serializer.serialize(gameState)) {
        std::cout << "Game saved successfully!" << std::endl;
    } else {
        std::cout << "Failed to save game!" << std::endl;
    }

    std::cout << "Loading game state..." << std::endl;
    jd::TownContext loadedState;
    if (serializer.deserialize(loadedState)) {
        if (loadedState.population == gameState.population && loadedState.land_acres == gameState.land_acres &&
            loadedState.wheat_bushels == gameState.wheat_bushels && loadedState.year == gameState.year) {

            std::cout << "Game loaded successfully!" << std::endl;
            std::cout << "Population: " << loadedState.population << std::endl;
            std::cout << "Lands: " << loadedState.land_acres << std::endl;
            std::cout << "Wheat: " << loadedState.wheat_bushels << std::endl;
            std::cout << "Year: " << loadedState.year << std::endl;
        } else {
            std::cout << "Failed to load game!" << std::endl;
        }
    } else {
        std::cout << "Failed to load game!" << std::endl;
    }
}

int main(void)
{
    test_save_manager();
    return 0;
}
