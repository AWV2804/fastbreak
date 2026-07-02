add_library(fastbreak_engine STATIC
  engine.cpp
  sim/possession_resolver.cpp
  game/game_simulator.cpp
  api/json.cpp
  api/simulation_engine.cpp
)

target_include_directories(fastbreak_engine
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_compile_features(fastbreak_engine PUBLIC cxx_std_20)
