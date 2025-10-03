#include <iostream>
#include <locale>
#include <memory>

#include "game_app.hpp"
#include "game_event_generator.hpp"
#include "path.h"

int main(void)
{
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
