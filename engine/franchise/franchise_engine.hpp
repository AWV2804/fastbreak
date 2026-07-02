#pragma once

#include "engine/core/rng.hpp"
#include "engine/game/game_state.hpp"
#include "engine/model/player.hpp"

#include <string>
#include <unordered_map>
#include <vector>

struct ScheduledGame {
    std::string game_id;
    std::string home_team_id;
    std::string away_team_id;
    int day = 0;
    bool simulated = false;
    int home_score = 0;
    int away_score = 0;
};

struct TeamStanding {
    std::string team_id;
    int wins = 0;
    int losses = 0;
    int points_for = 0;
    int points_against = 0;
};

struct FranchiseTeam {
    std::string team_id;
    std::string team_name;
    CourtLineup active_lineup;
    std::vector<std::string> roster_player_ids;
};

struct TradeProposal {
    std::string team_a_id;                      // Proposing team (usually user)
    std::string team_b_id;                      // Receiving team (usually AI)
    std::vector<std::string> assets_sent;      // Players Team A is giving
    std::vector<std::string> assets_received;  // Players Team B is giving
};

struct TradeResult {
    bool accepted = false;
    std::string reason = "";
    float value_sent = 0.0f;
    float value_received = 0.0f;
};

struct RookieProspect {
    std::string prospect_id;
    std::string name;
    Position position;
    int age = 19;
    Ratings<PlayerRatings> ratings;
};

class FranchiseEngine {
public:
    FranchiseEngine(LeagueConfig config, std::uint_fast32_t seed = 42U);

    void addTeam(const std::string& team_id, const std::string& team_name, const CourtLineup& lineup, const std::vector<std::string>& roster);
    void registerPlayer(const Player& player);

    // Season management
    void generateSchedule();
    void simulateDay(const std::string& player_team_id, std::vector<std::string>& out_logs);
    
    // Trading AI
    TradeResult evaluateTrade(const TradeProposal& proposal) const;
    bool executeTrade(const TradeProposal& proposal);

    // Rookie Draft
    void generateDraftClass();
    std::vector<std::string> getDraftOrder() const;
    bool executePlayerDraftPick(const std::string& player_team_id, const std::string& prospect_id);
    std::string executeAIDraftPick(const std::string& team_id);

    // Getters
    int currentDay() const { return current_day_; }
    const std::vector<ScheduledGame>& schedule() const { return schedule_; }
    std::vector<TeamStanding> getStandings() const;
    std::vector<Player> getTeamPlayers(const std::string& team_id) const;
    FranchiseTeam getTeam(const std::string& team_id) const;
    const std::vector<RookieProspect>& getDraftClass() const { return draft_class_; }

private:
    void autoSimulateGame(ScheduledGame& game);
    float calculateOffenseRating(const FranchiseTeam& team) const;
    float calculateDefenseRating(const FranchiseTeam& team) const;

    // Helper functions for Valuation
    float calculatePlayerAssetValue(const Player& p) const;
    Position getNeedPosition(const FranchiseTeam& team) const;
    std::string generateRandomName();

    LeagueConfig config_;
    RNG rng_;
    int current_day_ = 0;

    std::unordered_map<std::string, FranchiseTeam> teams_;
    std::unordered_map<std::string, Player> players_db_;
    std::vector<ScheduledGame> schedule_;
    std::unordered_map<std::string, TeamStanding> standings_;
    std::vector<RookieProspect> draft_class_;
};
