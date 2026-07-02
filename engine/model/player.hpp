#pragma once

#include <string>
#include <vector>
#include <optional>

enum class Position { PG, SG, SF, PF, C };

template <typename T>
struct Ratings {
    T current;
    T potential;
};

struct PlayerRatings {
    // Scoring
    float inside_scoring = 0.5f;   // Layups, dunks, post scoring
    float mid_range = 0.5f;        // Mid-range jumpers
    float three_point = 0.5f;      // Three-point shots
    float free_throw = 0.5f;       // Free-throw accuracy

    // Playmaking & Offense
    float passing = 0.5f;          // Pass accuracy and vision
    float dribbling = 0.5f;        // Ball handling and security
    float offensive_iq = 0.5f;     // Shot selection, off-ball movement

    // Defense
    float perimeter_defense = 0.5f;// Guarding PG/SG/SF on the perimeter
    float interior_defense = 0.5f; // Interior rim protection and post defense
    float stealing = 0.5f;         // Passing lanes interception, on-ball steals
    float blocking = 0.5f;         // Blocked shots

    // Physicals / Rebounding
    float rebounding = 0.5f;       // Offensive/defensive board crashing
    float speed = 0.5f;            // Transition and quickness
    float strength = 0.5f;         // Post backing, box out strength
    float stamina = 0.5f;          // Durability and fatigue resistance
};

struct Player {
    std::string player_id = "";
    std::string name = "";
    int age = 20;
    Position primary_position = Position::PG;

    Ratings<PlayerRatings> ratings{};
    
    float fatigue = 0.0f; // 0.0 = completely fresh, 1.0 = exhausted
};

struct TeamRoster {
    std::string team_id;
    std::vector<std::string> roster_player_ids;
};

struct LeagueConfig {
    int roster_limit = 15;
    int salary_cap = 140000000;
};
