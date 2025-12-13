cmake_minimum_required(VERSION 3.22)

set(PROJECT_NAME lab4)

project(${PROJECT_NAME} VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS true)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()


set(SRC_FILES
    src/allocator.cpp
)

add_executable(${PROJECT_NAME} main.cpp ${SRC_FILES})

target_include_directories(${PROJECT_NAME} PUBLIC include)
target_include_directories(${PROJECT_NAME} PRIVATE src)

set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 20)
target_compile_options(${PROJECT_NAME} PRIVATE -O2 -fPIC)

include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/5376968f6948923e2411081fd9372e71a59d8e77.zip
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Tests
add_executable(alloc_test src/allocator.cpp src/alloc_test.cpp)
target_include_directories(alloc_test PUBLIC include)
target_link_libraries(alloc_test gtest_main)
add_test(NAME alloc_test COMMAND alloc_test)
