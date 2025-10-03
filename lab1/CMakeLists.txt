cmake_minimum_required(VERSION 3.22)

include(FetchContent)

# Configure file to have relative path to the source dir
set(ROOT_INFO true)
if (ROOT_INFO)
    set(ROOT_DIR "${CMAKE_SOURCE_DIR}/")
else()
    set(ROOT_DIR "")
endif()

configure_file(${CMAKE_SOURCE_DIR}/include/path.h.in ${CMAKE_SOURCE_DIR}/include/path.h)

set(PROJECT_NAME lab1)

project(${PROJECT_NAME} VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS true)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

FetchContent_Declare(
    yaml-cpp
    GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
    GIT_TAG        yaml-cpp-0.7.0
)

set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "Disable building yaml-cpp tests" FORCE)
FetchContent_MakeAvailable(yaml-cpp)

set(SRC_FILES
    src/file/backend.cpp
    src/game_event_generator.cpp
    src/game_app.cpp
)

add_executable(${PROJECT_NAME} main.cpp ${SRC_FILES})
target_link_libraries(${PROJECT_NAME} PRIVATE yaml-cpp)

target_include_directories(${PROJECT_NAME} PUBLIC include)
target_include_directories(${PROJECT_NAME} PRIVATE src)

set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 20)
target_compile_options(${PROJECT_NAME} PRIVATE -O2 -fPIC)
