#pragma once

#include "engine/core/rng.hpp"
#include "engine/game/game_state.hpp"
#include "engine/game/manager_command.hpp"
#include "engine/model/player.hpp"

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

class GameSimulator {
public:
    explicit GameSimulator(const std::unordered_map<std::string, Player>& players);

    GameState startGame(const std::string& game_id,
                        const std::vector<std::string>& home_roster,
                        const std::vector<std::string>& away_roster,
                        const CourtLineup& home_lineup,
                        const CourtLineup& away_lineup,
                        std::uint_fast32_t seed);

    CommandValidation submitManagerCommand(int team_index, const ManagerCommand& command);
    bool advanceToNextDecisionPoint();

    const GameState& state() const { return state_; }

private:
    void triggerJumpBall();
    void processQuarterEnd();
    void logEvent(const std::string& description);

    std::unordered_map<std::string, Player> players_;
    GameState state_;
    RNG rng_;
};
