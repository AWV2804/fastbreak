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

float FranchiseEngine::calculatePlayerAssetValue(const Player& p) const {
    const auto& r = p.ratings.current;
    
    // Skill sum (15 ratings)
    float skill_sum = r.inside_scoring + r.mid_range + r.three_point + r.free_throw +
                      r.passing + r.dribbling + r.offensive_iq +
                      r.perimeter_defense + r.interior_defense + r.stealing + r.blocking +
                      r.rebounding + r.speed + r.strength + r.stamina;
                      
    float base_value = (skill_sum / 15.0f) * 100.0f; // Scale 0-100

    // Potential upside
    const auto& pot = p.ratings.potential;
    float pot_upside = (pot.inside_scoring - r.inside_scoring) + 
                       (pot.three_point - r.three_point) +
                       (pot.passing - r.passing);
    
    float potential_bonus = (pot_upside / 3.0f) * 40.0f;

    float total_value = base_value + potential_bonus;

    // Age multiplier
    if (p.age <= 21) {
        total_value += (23 - p.age) * 4.0f; // Youth bonus
    } else if (p.age >= 32) {
        total_value -= (p.age - 31) * 3.5f; // Aging discount
    }

    // Fatigue modifier
    total_value *= (1.0f - (p.fatigue * 0.1f));

    return std::max(10.0f, total_value);
}

Position FranchiseEngine::getNeedPosition(const FranchiseTeam& team) const {
    // Check ratings of starting lineup to find weakest position
    std::unordered_map<Position, float> starter_ratings;
    
    auto pg = players_db_.find(team.active_lineup.pg);
    auto sg = players_db_.find(team.active_lineup.sg);
    auto sf = players_db_.find(team.active_lineup.sf);
    auto pf = players_db_.find(team.active_lineup.pf);
    auto c  = players_db_.find(team.active_lineup.c);

    starter_ratings[Position::PG] = pg != players_db_.end() ? pg->second.ratings.current.passing : 0.0f;
    starter_ratings[Position::SG] = sg != players_db_.end() ? sg->second.ratings.current.three_point : 0.0f;
    starter_ratings[Position::SF] = sf != players_db_.end() ? sf->second.ratings.current.mid_range : 0.0f;
    starter_ratings[Position::PF] = pf != players_db_.end() ? pf->second.ratings.current.inside_scoring : 0.0f;
    starter_ratings[Position::C]  = c != players_db_.end() ? c->second.ratings.current.rebounding : 0.0f;

    Position weakest = Position::PG;
    float lowest_val = 999.0f;
    for (const auto& pair : starter_ratings) {
        if (pair.second < lowest_val) {
            lowest_val = pair.second;
            weakest = pair.first;
        }
    }
    return weakest;
}

std::string FranchiseEngine::generateRandomName() {
    std::vector<std::string> first_names = {
        "Cade", "Jalen", "Paolo", "LaMelo", "Anthony", 
        "Evan", "Jabari", "Scottie", "Bennedict", "Shaedon", 
        "Brandon", "Scoot", "Amen", "Ausar", "Keyonte"
    };
    std::vector<std::string> last_names = {
        "Cunningham", "Green", "Banchero", "Ball", "Edwards", 
        "Mobley", "Smith", "Barnes", "Mathurin", "Sharpe", 
        "Miller", "Henderson", "Thompson", "George", "Dick"
    };

    int idx_f = static_cast<int>(rng_.uniform(0.0f, static_cast<float>(first_names.size())));
    int idx_l = static_cast<int>(rng_.uniform(0.0f, static_cast<float>(last_names.size())));
    
    idx_f = std::clamp(idx_f, 0, static_cast<int>(first_names.size()) - 1);
    idx_l = std::clamp(idx_l, 0, static_cast<int>(last_names.size()) - 1);

    return first_names[idx_f] + " " + last_names[idx_l];
}

TradeResult FranchiseEngine::evaluateTrade(const TradeProposal& proposal) const {
    TradeResult result;
    
    auto it_a = teams_.find(proposal.team_a_id);
    auto it_b = teams_.find(proposal.team_b_id);
    if (it_a == teams_.end() || it_b == teams_.end()) {
        result.accepted = false;
        result.reason = "Invalid team ID(s).";
        return result;
    }

    const FranchiseTeam& team_b = it_b->second;

    // Roster size check for team B (AI)
    int size_diff = static_cast<int>(proposal.assets_sent.size()) - static_cast<int>(proposal.assets_received.size());
    if (static_cast<int>(team_b.roster_player_ids.size()) + size_diff > config_.roster_limit) {
        result.accepted = false;
        result.reason = "AI roster limit would be exceeded.";
        return result;
    }

    Position b_need = getNeedPosition(team_b);

    // Calculate value gained by B (proposer sent)
    float val_gained = 0.0f;
    for (const auto& pid : proposal.assets_sent) {
        auto pit = players_db_.find(pid);
        if (pit != players_db_.end()) {
            float val = calculatePlayerAssetValue(pit->second);
            // Apply positional need bonus (15% premium)
            if (pit->second.primary_position == b_need) {
                val *= 1.15f;
            }
            val_gained += val;
        }
    }

    // Calculate value lost by B (proposer received)
    float val_lost = 0.0f;
    for (const auto& pid : proposal.assets_received) {
        auto pit = players_db_.find(pid);
        if (pit != players_db_.end()) {
            val_lost += calculatePlayerAssetValue(pit->second);
        }
    }

    result.value_sent = val_gained;
    result.value_received = val_lost;

    // AI requires a 6% premium to accept trades (conservative trading)
    if (val_gained >= val_lost * 1.06f) {
        result.accepted = true;
        result.reason = "Trade accepted!";
    } else {
        result.accepted = false;
        result.reason = "AI team rejects. Gained value (" + std::to_string(static_cast<int>(val_gained)) + 
                        ") is less than value lost (" + std::to_string(static_cast<int>(val_lost)) + ") with trade premium.";
    }

    return result;
}

bool FranchiseEngine::executeTrade(const TradeProposal& proposal) {
    TradeResult result = evaluateTrade(proposal);
    if (!result.accepted) return false;

    FranchiseTeam& team_a = teams_[proposal.team_a_id];
    FranchiseTeam& team_b = teams_[proposal.team_b_id];

    // Remove from A, add to B
    for (const auto& pid : proposal.assets_sent) {
        team_a.roster_player_ids.erase(
            std::remove(team_a.roster_player_ids.begin(), team_a.roster_player_ids.end(), pid),
            team_a.roster_player_ids.end()
        );
        team_b.roster_player_ids.push_back(pid);

        // Lineup cleanup if traded starter
        if (team_a.active_lineup.pg == pid) team_a.active_lineup.pg = "";
        else if (team_a.active_lineup.sg == pid) team_a.active_lineup.sg = "";
        else if (team_a.active_lineup.sf == pid) team_a.active_lineup.sf = "";
        else if (team_a.active_lineup.pf == pid) team_a.active_lineup.pf = "";
        else if (team_a.active_lineup.c == pid) team_a.active_lineup.c = "";
    }

    // Remove from B, add to A
    for (const auto& pid : proposal.assets_received) {
        team_b.roster_player_ids.erase(
            std::remove(team_b.roster_player_ids.begin(), team_b.roster_player_ids.end(), pid),
            team_b.roster_player_ids.end()
        );
        team_a.roster_player_ids.push_back(pid);

        // Lineup cleanup
        if (team_b.active_lineup.pg == pid) team_b.active_lineup.pg = "";
        else if (team_b.active_lineup.sg == pid) team_b.active_lineup.sg = "";
        else if (team_b.active_lineup.sf == pid) team_b.active_lineup.sf = "";
        else if (team_b.active_lineup.pf == pid) team_b.active_lineup.pf = "";
        else if (team_b.active_lineup.c == pid) team_b.active_lineup.c = "";
    }

    // Starting lineup auto-refills for empty spots
    std::vector<FranchiseTeam*> team_ptrs = {&team_a, &team_b};
    for (auto* t : team_ptrs) {
        std::vector<Position> positions = {Position::PG, Position::SG, Position::SF, Position::PF, Position::C};
        for (auto pos : positions) {
            std::string current_starter = t->active_lineup.get_player_at_position(pos);
            if (current_starter.empty() || std::find(t->roster_player_ids.begin(), t->roster_player_ids.end(), current_starter) == t->roster_player_ids.end()) {
                // Find next best player on roster for this position
                std::string best_id = "";
                float best_rating = -1.0f;
                for (const auto& r_id : t->roster_player_ids) {
                    // Check if already starting in another position
                    if (t->active_lineup.has_player(r_id)) continue;
                    
                    auto pit = players_db_.find(r_id);
                    if (pit != players_db_.end()) {
                        if (pit->second.primary_position == pos && calculatePlayerAssetValue(pit->second) > best_rating) {
                            best_rating = calculatePlayerAssetValue(pit->second);
                            best_id = r_id;
                        }
                    }
                }
                
                // Fallback: take any available player not starting
                if (best_id.empty()) {
                    for (const auto& r_id : t->roster_player_ids) {
                        if (!t->active_lineup.has_player(r_id)) {
                            best_id = r_id;
                            break;
                        }
                    }
                }

                t->active_lineup.set_player_at_position(pos, best_id);
            }
        }
    }

    return true;
}

void FranchiseEngine::generateDraftClass() {
    draft_class_.clear();
    int num_teams = static_cast<int>(teams_.size());
    int prospects_count = num_teams * 2; // 2 rounds of draft class

    for (int i = 0; i < prospects_count; ++i) {
        RookieProspect p;
        p.prospect_id = "R_" + std::to_string(i + 1);
        p.name = generateRandomName();
        p.age = 19 + static_cast<int>(rng_.uniform(0.0f, 3.0f)); // 19-21
        
        // Cycle positions
        p.position = static_cast<Position>(i % 5);

        // Randomized attributes
        float current_avg = rng_.uniform(0.40f, 0.55f);
        float pot_avg = current_avg + rng_.uniform(0.08f, 0.26f);

        p.ratings.current.inside_scoring = current_avg;
        p.ratings.current.mid_range = current_avg;
        p.ratings.current.three_point = current_avg;
        p.ratings.current.free_throw = current_avg;
        p.ratings.current.passing = current_avg;
        p.ratings.current.dribbling = current_avg;
        p.ratings.current.offensive_iq = current_avg;
        p.ratings.current.perimeter_defense = current_avg;
        p.ratings.current.interior_defense = current_avg;
        p.ratings.current.stealing = current_avg;
        p.ratings.current.blocking = current_avg;
        p.ratings.current.rebounding = current_avg;
        p.ratings.current.speed = current_avg;
        p.ratings.current.strength = current_avg;
        p.ratings.current.stamina = current_avg;

        p.ratings.potential.inside_scoring = pot_avg;
        p.ratings.potential.mid_range = pot_avg;
        p.ratings.potential.three_point = pot_avg;
        p.ratings.potential.free_throw = pot_avg;
        p.ratings.potential.passing = pot_avg;
        p.ratings.potential.dribbling = pot_avg;
        p.ratings.potential.offensive_iq = pot_avg;
        p.ratings.potential.perimeter_defense = pot_avg;
        p.ratings.potential.interior_defense = pot_avg;
        p.ratings.potential.stealing = pot_avg;
        p.ratings.potential.blocking = pot_avg;
        p.ratings.potential.rebounding = pot_avg;
        p.ratings.potential.speed = pot_avg;
        p.ratings.potential.strength = pot_avg;
        p.ratings.potential.stamina = pot_avg;

        draft_class_.push_back(p);
    }
}

std::vector<std::string> FranchiseEngine::getDraftOrder() const {
    std::vector<TeamStanding> standings = getStandings();
    
    // Draft order is the reverse of standings (worst team first)
    std::reverse(standings.begin(), standings.end());
    
    std::vector<std::string> order;
    for (const auto& stand : standings) {
        order.push_back(stand.team_id);
    }
    return order;
}

bool FranchiseEngine::executePlayerDraftPick(const std::string& player_team_id, const std::string& prospect_id) {
    auto it = std::find_if(draft_class_.begin(), draft_class_.end(), [&](const RookieProspect& p) {
        return p.prospect_id == prospect_id;
    });

    if (it == draft_class_.end()) return false;

    // Register prospect as active player in the league
    Player player;
    player.player_id = it->prospect_id;
    player.name = it->name;
    player.age = it->age;
    player.primary_position = it->position;
    player.ratings = it->ratings;
    player.fatigue = 0.0f;

    registerPlayer(player);

    // Add to roster
    teams_[player_team_id].roster_player_ids.push_back(it->prospect_id);
    
    // Remove from draft class list
    draft_class_.erase(it);
    return true;
}

std::string FranchiseEngine::executeAIDraftPick(const std::string& team_id) {
    if (draft_class_.empty()) return "";

    FranchiseTeam& team = teams_[team_id];
    Position need = getNeedPosition(team);

    // Find best available player filling a need, fallback to best overall
    auto best_it = draft_class_.begin();
    float best_score = -1.0f;

    for (auto it = draft_class_.begin(); it != draft_class_.end(); ++it) {
        // Valuation: current + potential weight
        float score = (it->ratings.current.mid_range + it->ratings.potential.mid_range * 0.5f) * 100.0f;
        if (it->position == need) {
            score += 15.0f; // Need bonus
        }

        if (score > best_score) {
            best_score = score;
            best_it = it;
        }
    }

    std::string drafted_name = best_it->name;
    std::string drafted_id = best_it->prospect_id;

    // Register prospect in league players
    Player player;
    player.player_id = drafted_id;
    player.name = drafted_name;
    player.age = best_it->age;
    player.primary_position = best_it->position;
    player.ratings = best_it->ratings;
    player.fatigue = 0.0f;

    registerPlayer(player);

    // Add to roster
    team.roster_player_ids.push_back(drafted_id);

    // Remove from draft class
    draft_class_.erase(best_it);

    return drafted_name;
}
