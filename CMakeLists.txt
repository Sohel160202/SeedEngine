cmake_minimum_required(VERSION 3.24)

project(SeedEngine VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(SEED_BUILD_TESTS "Build Seed Engine tests" ON)

add_subdirectory(engine)
add_subdirectory(runtime)
add_subdirectory(editor)

if(SEED_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
