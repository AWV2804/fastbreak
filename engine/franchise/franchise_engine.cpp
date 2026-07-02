#include "engine/franchise/franchise_engine.hpp"

#include <algorithm>
#include <sstream>

FranchiseEngine::FranchiseEngine(LeagueConfig config, std::uint_fast32_t seed)
    : config_(config), rng_(seed), current_day_(0) {}

void FranchiseEngine::addTeam(const std::string& team_id, 
                              const std::string& team_name, 
                              const CourtLineup& lineup, 
                              const std::vector<std::string>& roster) {
    FranchiseTeam team;
    team.team_id = team_id;
    team.team_name = team_name;
    team.active_lineup = lineup;
    team.roster_player_ids = roster;

    teams_[team_id] = team;

    TeamStanding standing;
    standing.team_id = team_id;
    standings_[team_id] = standing;
}

void FranchiseEngine::registerPlayer(const Player& player) {
    players_db_[player.player_id] = player;
}

std::vector<Player> FranchiseEngine::getTeamPlayers(const std::string& team_id) const {
    std::vector<Player> list;
    auto it = teams_.find(team_id);
    if (it != teams_.end()) {
        for (const auto& pid : it->second.roster_player_ids) {
            auto pit = players_db_.find(pid);
            if (pit != players_db_.end()) {
                list.push_back(pit->second);
            }
        }
    }
    return list;
}

FranchiseTeam FranchiseEngine::getTeam(const std::string& team_id) const {
    auto it = teams_.find(team_id);
    if (it != teams_.end()) {
        return it->second;
    }
    return FranchiseTeam{};
}

void FranchiseEngine::generateSchedule() {
    schedule_.clear();
    std::vector<std::string> team_ids;
    for (const auto& pair : teams_) {
        team_ids.push_back(pair.first);
    }

    int num_teams = static_cast<int>(team_ids.size());
    if (num_teams < 2) return;

    // If odd number of teams, we would need a bye, but we assume even for now.
    // Berger tables / Circle method for round-robin
    int rounds = num_teams - 1;
    int games_per_round = num_teams / 2;

    int day_counter = 1;

    // Home games first half
    for (int round = 0; round < rounds; ++round) {
        for (int game_idx = 0; game_idx < games_per_round; ++game_idx) {
            int home = (round + game_idx) % (num_teams - 1);
            int away = (num_teams - 1 - game_idx + round) % (num_teams - 1);
            
            if (game_idx == 0) {
                away = num_teams - 1;
            }

            ScheduledGame game;
            game.game_id = "G_" + std::to_string(day_counter) + "_" + std::to_string(game_idx);
            game.home_team_id = team_ids[home];
            game.away_team_id = team_ids[away];
            game.day = day_counter;
            game.simulated = false;
            
            schedule_.push_back(game);
        }
        day_counter++;
    }

    // Away games second half (mirroring with home/away swapped)
    for (int round = 0; round < rounds; ++round) {
        for (int game_idx = 0; game_idx < games_per_round; ++game_idx) {
            int home = (round + game_idx) % (num_teams - 1);
            int away = (num_teams - 1 - game_idx + round) % (num_teams - 1);
            
            if (game_idx == 0) {
                away = num_teams - 1;
            }

            ScheduledGame game;
            game.game_id = "G_" + std::to_string(day_counter) + "_" + std::to_string(game_idx);
            // Swapped home and away
            game.home_team_id = team_ids[away];
            game.away_team_id = team_ids[home];
            game.day = day_counter;
            game.simulated = false;

            schedule_.push_back(game);
        }
        day_counter++;
    }

    current_day_ = 1;
}

float FranchiseEngine::calculateOffenseRating(const FranchiseTeam& team) const {
    const CourtLineup& l = team.active_lineup;
    std::vector<std::string> pids = {l.pg, l.sg, l.sf, l.pf, l.c};
    float total = 0.0f;
    int count = 0;

    for (const auto& pid : pids) {
        auto it = players_db_.find(pid);
        if (it != players_db_.end()) {
            const Player& p = it->second;
            // Offense score = weight mid/inside/3pt/passing
            float p_off = p.ratings.current.inside_scoring * 0.25f + 
                          p.ratings.current.mid_range * 0.25f + 
                          p.ratings.current.three_point * 0.25f + 
                          p.ratings.current.passing * 0.25f;
            // Apply fatigue penalty
            p_off *= (1.0f - (p.fatigue * 0.15f));
            total += p_off;
            count++;
        }
    }
    return count > 0 ? (total / static_cast<float>(count)) : 0.5f;
}

float FranchiseEngine::calculateDefenseRating(const FranchiseTeam& team) const {
    const CourtLineup& l = team.active_lineup;
    std::vector<std::string> pids = {l.pg, l.sg, l.sf, l.pf, l.c};
    float total = 0.0f;
    int count = 0;

    for (const auto& pid : pids) {
        auto it = players_db_.find(pid);
        if (it != players_db_.end()) {
            const Player& p = it->second;
            float p_def = p.ratings.current.perimeter_defense * 0.3f + 
                          p.ratings.current.interior_defense * 0.3f + 
                          p.ratings.current.stealing * 0.2f + 
                          p.ratings.current.blocking * 0.2f;
            p_def *= (1.0f - (p.fatigue * 0.10f));
            total += p_def;
            count++;
        }
    }
    return count > 0 ? (total / static_cast<float>(count)) : 0.5f;
}

void FranchiseEngine::autoSimulateGame(ScheduledGame& game) {
    auto home_team = teams_[game.home_team_id];
    auto away_team = teams_[game.away_team_id];

    float home_off = calculateOffenseRating(home_team);
    float home_def = calculateDefenseRating(home_team);
    float away_off = calculateOffenseRating(away_team);
    float away_def = calculateDefenseRating(away_team);

    // Score calculations
    float home_base = 100.0f + (home_off - away_def) * 35.0f + 3.0f + rng_.uniform(-10.0f, 10.0f); // +3 home court advantage
    float away_base = 100.0f + (away_off - home_def) * 35.0f + rng_.uniform(-10.0f, 10.0f);

    int home_score = std::max(70, static_cast<int>(home_base));
    int away_score = std::max(70, static_cast<int>(away_base));

    // Resolve ties (overtime)
    if (home_score == away_score) {
        if (rng_.uniform() < 0.5f) {
            home_score += static_cast<int>(rng_.uniform(2.0f, 7.0f));
        } else {
            away_score += static_cast<int>(rng_.uniform(2.0f, 7.0f));
        }
    }

    game.home_score = home_score;
    game.away_score = away_score;
    game.simulated = true;

    // Update Standings
    TeamStanding& home_stand = standings_[game.home_team_id];
    TeamStanding& away_stand = standings_[game.away_team_id];

    home_stand.points_for += home_score;
    home_stand.points_against += away_score;
    away_stand.points_for += away_score;
    away_stand.points_against += home_score;

    if (home_score > away_score) {
        home_stand.wins++;
        away_stand.losses++;
    } else {
        away_stand.wins++;
        home_stand.losses++;
    }

    // Fatigue simulated game impact: increase fatigue slightly for playing roster
    for (const auto& pid : home_team.roster_player_ids) {
        players_db_[pid].fatigue = std::clamp(players_db_[pid].fatigue + 0.18f, 0.0f, 1.0f);
    }
    for (const auto& pid : away_team.roster_player_ids) {
        players_db_[pid].fatigue = std::clamp(players_db_[pid].fatigue + 0.18f, 0.0f, 1.0f);
    }
}

void FranchiseEngine::simulateDay(const std::string& player_team_id, std::vector<std::string>& out_logs) {
    out_logs.clear();
    std::ostringstream ss;
    ss << "--- Day " << current_day_ << " ---";
    out_logs.push_back(ss.str());

    bool player_has_game = false;
    std::string player_game_opponent = "";

    for (auto& game : schedule_) {
        if (game.day == current_day_) {
            if (game.simulated) continue;

            // Check if it is the player's game
            if (game.home_team_id == player_team_id || game.away_team_id == player_team_id) {
                player_has_game = true;
                player_game_opponent = (game.home_team_id == player_team_id) ? game.away_team_id : game.home_team_id;
                
                // We auto-simulate it here if they don't play it manually
                autoSimulateGame(game);
                
                std::ostringstream log;
                log << "[USER GAME] " << teams_[game.home_team_id].team_name << " " << game.home_score
                    << " - " << game.away_score << " " << teams_[game.away_team_id].team_name << " (Simulated)";
                out_logs.push_back(log.str());
            } else {
                // AI vs AI auto-simulation
                autoSimulateGame(game);
                
                std::ostringstream log;
                log << teams_[game.home_team_id].team_name << " " << game.home_score
                    << " - " << game.away_score << " " << teams_[game.away_team_id].team_name;
                out_logs.push_back(log.str());
            }
        }
    }

    if (!player_has_game) {
        out_logs.push_back("Your team had a rest day.");
    }

    // All players in the league recover stamina slightly overnight (rest recovery)
    for (auto& pair : players_db_) {
        pair.second.fatigue = std::max(0.0f, pair.second.fatigue - 0.25f);
    }

    current_day_++;
}

std::vector<TeamStanding> FranchiseEngine::getStandings() const {
    std::vector<TeamStanding> list;
    for (const auto& pair : standings_) {
        list.push_back(pair.second);
    }

    // Sort by wins desc, then by point diff desc
    std::sort(list.begin(), list.end(), [](const TeamStanding& a, const TeamStanding& b) {
        if (a.wins != b.wins) {
            return a.wins > b.wins;
        }
        int diff_a = a.points_for - a.points_against;
        int diff_b = b.points_for - b.points_against;
        return diff_a > diff_b;
    });

    return list;
}
