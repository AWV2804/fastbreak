#include "engine/api/simulation_engine.hpp"

SimulationEngine::SimulationEngine(LeagueConfig config, const std::unordered_map<std::string, Player>& players)
    : config_(config), players_(players) {
    game_simulator_ = std::make_unique<GameSimulator>(players_);
}

void SimulationEngine::startGame(const std::string& game_id,
                               const std::vector<std::string>& home_roster,
                               const std::vector<std::string>& away_roster,
                               const CourtLineup& home_lineup,
                               const CourtLineup& away_lineup,
                               std::uint_fast32_t seed) {
    game_simulator_->startGame(game_id, home_roster, away_roster, home_lineup, away_lineup, seed);
}

CommandValidation SimulationEngine::submitManagerCommand(int team_index, const ManagerCommand& command) {
    return game_simulator_->submitManagerCommand(team_index, command);
}

bool SimulationEngine::advanceToNextDecisionPoint() {
    return game_simulator_->advanceToNextDecisionPoint();
}

GameSnapshotDTO SimulationEngine::getGameSnapshot() const {
    const GameState& state = game_simulator_->state();
    GameSnapshotDTO dto;
    
    dto.game_id = state.game_id;
    dto.quarter = state.quarter;
    dto.game_clock = state.game_clock;
    dto.shot_clock = state.shot_clock;
    
    dto.home_score = state.score[0];
    dto.away_score = state.score[1];
    
    dto.home_fouls = state.team_fouls[0];
    dto.away_fouls = state.team_fouls[1];
    
    dto.home_timeouts = state.timeouts_remaining[0];
    dto.away_timeouts = state.timeouts_remaining[1];
    
    dto.possession = (state.possession_team_index == 0) ? "HOME" : "AWAY";
    
    // Resolve ball handler name
    auto it = players_.find(state.ball_handler_id);
    if (it != players_.end()) {
        dto.ball_handler = it->second.name;
    } else {
        dto.ball_handler = state.ball_handler_id;
    }
    
    dto.game_over = state.game_over;

    // Send latest 15 events
    int total_events = static_cast<int>(state.events.size());
    int start_idx = std::max(0, total_events - 15);
    for (int i = start_idx; i < total_events; ++i) {
        dto.latest_events.push_back(state.events[i].description);
    }

    if (state.decision_point.has_value()) {
        dto.decision_point.awaiting_command = state.decision_point->awaiting_command;
        dto.decision_point.offense_team_index = state.decision_point->offense_team_index;
        dto.decision_point.defense_team_index = state.decision_point->defense_team_index;
    } else {
        dto.decision_point.awaiting_command = false;
        dto.decision_point.offense_team_index = 0;
        dto.decision_point.defense_team_index = 0;
    }

    return dto;
}
