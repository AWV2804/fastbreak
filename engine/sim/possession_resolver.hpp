#pragma once

#include "engine/core/rng.hpp"
#include "engine/game/game_state.hpp"
#include "engine/model/player.hpp"

#include <string>
#include <unordered_map>

struct PlayActionResult {
    std::string description = "";
    bool possession_ended = false;
    bool shot_attempted = false;
    bool shot_made = false;
    int points_scored = 0;
};

class PossessionResolver {
public:
    PossessionResolver(GameState& state, 
                       const std::unordered_map<std::string, Player>& player_db, 
                       RNG& rng);

    PlayActionResult resolveNextAction();

private:
    std::string selectRandomTeammate(const CourtLineup& lineup, const std::string& current_handler_id);
    const Player* getPlayerOrNull(const std::string& id) const;
    PlayerGameStats& getOrCreateStats(const std::string& player_id);

    GameState& state_;
    const std::unordered_map<std::string, Player>& player_db_;
    RNG& rng_;
};
