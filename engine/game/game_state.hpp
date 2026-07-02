#pragma once

#include "engine/game/manager_command.hpp"
#include "engine/model/player.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include <unordered_map>

enum class TeamIndex { HOME = 0, AWAY = 1 };
enum class StrategyPace { SLOW, NEUTRAL, FAST };
enum class StrategyOffense { BALANCED, INSIDE, PERIMETER };
enum class StrategyDefense { MAN_TO_MAN, ZONE, PRESS };

struct CourtLineup {
    std::string pg = "";
    std::string sg = "";
    std::string sf = "";
    std::string pf = "";
    std::string c = "";

    bool has_player(const std::string& id) const {
        return pg == id || sg == id || sf == id || pf == id || c == id;
    }

    std::string get_player_at_position(Position pos) const {
        switch (pos) {
            case Position::PG: return pg;
            case Position::SG: return sg;
            case Position::SF: return sf;
            case Position::PF: return pf;
            case Position::C:  return c;
        }
        return "";
    }

    void set_player_at_position(Position pos, const std::string& id) {
        switch (pos) {
            case Position::PG: pg = id; break;
            case Position::SG: sg = id; break;
            case Position::SF: sf = id; break;
            case Position::PF: pf = id; break;
            case Position::C:  c = id; break;
        }
    }
};

struct PlayerGameStats {
    std::string player_id;
    int points = 0;
    int fg_made = 0;
    int fg_attempted = 0;
    int tp_made = 0;
    int tp_attempted = 0;
    int ft_made = 0;
    int ft_attempted = 0;
    int rebounds_off = 0;
    int rebounds_def = 0;
    int assists = 0;
    int steals = 0;
    int blocks = 0;
    int turnovers = 0;
    int fouls = 0;
    float minutes_played = 0.0f;
};

struct GameEventLog {
    int quarter = 1;
    float clock = 720.0f;
    std::string description = "";
};

struct DecisionPoint {
    bool awaiting_command = true;
    int offense_team_index = 0;
    int defense_team_index = 1;
};

struct TeamStrategy {
    StrategyPace pace = StrategyPace::NEUTRAL;
    StrategyOffense offense = StrategyOffense::BALANCED;
    StrategyDefense defense = StrategyDefense::MAN_TO_MAN;
};

struct GameState {
    std::string game_id = "";
    int quarter = 1;
    float game_clock = 720.0f;     // Seconds remaining in quarter (e.g. 12 minutes = 720s)
    float shot_clock = 24.0f;      // Seconds remaining on shot clock

    std::array<int, 2> score{0, 0};
    std::array<int, 2> team_fouls{0, 0};
    std::array<int, 2> timeouts_remaining{7, 7};

    std::array<CourtLineup, 2> lineups;
    std::array<std::vector<std::string>, 2> rosters; // List of all player IDs per team
    
    std::unordered_map<std::string, PlayerGameStats> player_stats;

    int possession_team_index = 0; // 0 = HOME, 1 = AWAY
    std::string ball_handler_id = "";

    std::array<TeamStrategy, 2> strategies;

    std::vector<GameEventLog> events;
    std::optional<DecisionPoint> decision_point;
    bool game_over = false;
    std::uint_fast32_t seed = 0;
};
