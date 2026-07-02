cmake_minimum_required(VERSION 3.20)

project(fastbreak VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (MSVC)
    add_compile_options(/W4 /permissive-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_subdirectory(engine)

# Smoke test executable
add_executable(fastbreak_smoke tests/smoke.cpp)
target_link_libraries(fastbreak_smoke PRIVATE fastbreak_engine)

# Subprocess bridge executable for Unity
add_executable(fastbreak_bridge app/bridge_main.cpp)
target_link_libraries(fastbreak_bridge PRIVATE fastbreak_engine)

enable_testing()
add_test(NAME smoke_test COMMAND fastbreak_smoke)
