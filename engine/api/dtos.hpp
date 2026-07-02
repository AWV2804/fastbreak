#pragma once

#include "engine/game/game_state.hpp"
#include "engine/model/player.hpp"

#include <string>
#include <vector>

struct DecisionPointDTO {
    bool awaiting_command = false;
    int offense_team_index = 0;
    int defense_team_index = 0;
};

struct GameSnapshotDTO {
    std::string game_id = "";
    int quarter = 1;
    float game_clock = 720.0f;
    float shot_clock = 24.0f;
    int home_score = 0;
    int away_score = 0;
    int home_fouls = 0;
    int away_fouls = 0;
    int home_timeouts = 0;
    int away_timeouts = 0;
    std::string possession = "HOME";
    std::string ball_handler = "";
    bool game_over = false;
    std::vector<std::string> latest_events;
    DecisionPointDTO decision_point;
};
