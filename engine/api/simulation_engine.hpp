#pragma once

#include "engine/api/dtos.hpp"
#include "engine/game/game_simulator.hpp"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class SimulationEngine {
public:
    SimulationEngine(LeagueConfig config, const std::unordered_map<std::string, Player>& players);

    void startGame(const std::string& game_id,
                   const std::vector<std::string>& home_roster,
                   const std::vector<std::string>& away_roster,
                   const CourtLineup& home_lineup,
                   const CourtLineup& away_lineup,
                   std::uint_fast32_t seed);

    CommandValidation submitManagerCommand(int team_index, const ManagerCommand& command);
    bool advanceToNextDecisionPoint();

    GameSnapshotDTO getGameSnapshot() const;

private:
    std::unique_ptr<GameSimulator> game_simulator_;
    LeagueConfig config_;
    std::unordered_map<std::string, Player> players_;
};
