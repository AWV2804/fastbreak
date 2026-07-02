#include "engine/sim/possession_resolver.hpp"

#include <algorithm>
#include <sstream>
#include <iomanip>

PossessionResolver::PossessionResolver(GameState& state, 
                                       const std::unordered_map<std::string, Player>& player_db, 
                                       RNG& rng)
    : state_(state), player_db_(player_db), rng_(rng) {}

const Player* PossessionResolver::getPlayerOrNull(const std::string& id) const {
    auto it = player_db_.find(id);
    if (it != player_db_.end()) {
        return &it->second;
    }
    return nullptr;
}

PlayerGameStats& PossessionResolver::getOrCreateStats(const std::string& player_id) {
    auto it = state_.player_stats.find(player_id);
    if (it != state_.player_stats.end()) {
        return it->second;
    }
    PlayerGameStats new_stats;
    new_stats.player_id = player_id;
    state_.player_stats[player_id] = new_stats;
    return state_.player_stats[player_id];
}

std::string PossessionResolver::selectRandomTeammate(const CourtLineup& lineup, const std::string& current_handler_id) {
    std::vector<std::string> teammates;
    if (!lineup.pg.empty() && lineup.pg != current_handler_id) teammates.push_back(lineup.pg);
    if (!lineup.sg.empty() && lineup.sg != current_handler_id) teammates.push_back(lineup.sg);
    if (!lineup.sf.empty() && lineup.sf != current_handler_id) teammates.push_back(lineup.sf);
    if (!lineup.pf.empty() && lineup.pf != current_handler_id) teammates.push_back(lineup.pf);
    if (!lineup.c.empty() && lineup.c != current_handler_id) teammates.push_back(lineup.c);

    if (teammates.empty()) return current_handler_id;

    // Weight selection slightly based on position
    int idx = static_cast<int>(rng_.uniform(0.0f, static_cast<float>(teammates.size())));
    return teammates[std::clamp(idx, 0, static_cast<int>(teammates.size()) - 1)];
}

PlayActionResult PossessionResolver::resolveNextAction() {
    PlayActionResult result;
    
    int offense_idx = state_.possession_team_index;
    int defense_idx = 1 - offense_idx;

    const CourtLineup& off_lineup = state_.lineups[offense_idx];
    const CourtLineup& def_lineup = state_.lineups[defense_idx];

    // Ensure we have a ball handler
    if (state_.ball_handler_id.empty()) {
        state_.ball_handler_id = off_lineup.pg.empty() ? off_lineup.sg : off_lineup.pg;
    }

    std::string handler_id = state_.ball_handler_id;
    const Player* handler = getPlayerOrNull(handler_id);
    if (!handler) {
        result.description = "No active ball handler found.";
        return result;
    }

    // Determine strategy modifiers
    const TeamStrategy& off_strat = state_.strategies[offense_idx];

    // Time deduction
    float elapsed = rng_.uniform(2.0f, 4.5f);
    // Adjust elapsed based on pace
    if (off_strat.pace == StrategyPace::FAST) {
        elapsed *= 0.75f;
    } else if (off_strat.pace == StrategyPace::SLOW) {
        elapsed *= 1.25f;
    }

    state_.game_clock -= elapsed;
    state_.shot_clock -= elapsed;

    if (state_.game_clock <= 0.0f) {
        state_.game_clock = 0.0f;
        result.description = "Quarter buzzer sounds!";
        result.possession_ended = true;
        return result;
    }

    // Decide Action
    float handler_dribbling = handler->ratings.current.dribbling;
    float turnover_prob = 0.09f - (handler_dribbling * 0.06f);
    
    // Shot clock pressure forces shots
    float shot_prob = 0.22f;
    if (state_.shot_clock <= 5.0f) {
        shot_prob = 0.65f;
    }
    if (state_.shot_clock <= 1.5f) {
        shot_prob = 0.90f;
    }

    float roll = rng_.uniform();

    if (roll < turnover_prob) {
        // TURNOVER
        getOrCreateStats(handler_id).turnovers++;
        
        // Check for steal
        std::string defender_id = def_lineup.pg; // default stealer
        float steal_roll = rng_.uniform();
        if (steal_roll < 0.4f && !def_lineup.sg.empty()) defender_id = def_lineup.sg;
        else if (steal_roll < 0.7f && !def_lineup.sf.empty()) defender_id = def_lineup.sf;

        const Player* defender = getPlayerOrNull(defender_id);
        std::ostringstream desc;
        desc << handler->name << " turned the ball over";
        
        if (defender && rng_.uniform() < defender->ratings.current.stealing) {
            getOrCreateStats(defender_id).steals++;
            desc << " (stolen by " << defender->name << ")";
        } else {
            desc << " on a bad pass";
        }

        result.description = desc.str();
        result.possession_ended = true;

        // Reset clocks and flip possession
        state_.possession_team_index = defense_idx;
        state_.shot_clock = 24.0f;
        state_.ball_handler_id = def_lineup.pg.empty() ? def_lineup.sg : def_lineup.pg;

    } else if (roll < (turnover_prob + shot_prob)) {
        // SHOT ATTEMPT
        result.shot_attempted = true;
        
        // Choose shot type
        // Positions PG/SG/SF shoot more 3pt, PF/C shoot more inside
        float three_chance = 0.35f;
        float inside_chance = 0.25f;

        if (handler->primary_position == Position::PF || handler->primary_position == Position::C) {
            three_chance = 0.08f;
            inside_chance = 0.62f;
        }

        // Adjust based on offensive strategy
        if (off_strat.offense == StrategyOffense::PERIMETER) {
            three_chance += 0.15f;
            inside_chance -= 0.10f;
        } else if (off_strat.offense == StrategyOffense::INSIDE) {
            three_chance -= 0.15f;
            inside_chance += 0.15f;
        }

        float shot_roll = rng_.uniform();
        enum class ShotType { INSIDE, MID_RANGE, THREE_POINT } shot_type;

        if (shot_roll < inside_chance) {
            shot_type = ShotType::INSIDE;
        } else if (shot_roll < (inside_chance + three_chance)) {
            shot_type = ShotType::THREE_POINT;
        } else {
            shot_type = ShotType::MID_RANGE;
        }

        // Determine defender contesting
        std::string defender_id = "";
        switch (handler->primary_position) {
            case Position::PG: defender_id = def_lineup.pg; break;
            case Position::SG: defender_id = def_lineup.sg; break;
            case Position::SF: defender_id = def_lineup.sf; break;
            case Position::PF: defender_id = def_lineup.pf; break;
            case Position::C:  defender_id = def_lineup.c;  break;
        }
        if (defender_id.empty()) defender_id = def_lineup.c.empty() ? def_lineup.pf : def_lineup.c;

        const Player* defender = getPlayerOrNull(defender_id);
        float def_rating = defender ? (shot_type == ShotType::INSIDE ? defender->ratings.current.interior_defense : defender->ratings.current.perimeter_defense) : 0.4f;

        // Check for block
        if (shot_type == ShotType::INSIDE && defender && rng_.uniform() < (defender->ratings.current.blocking * 0.18f)) {
            getOrCreateStats(defender_id).blocks++;
            getOrCreateStats(handler_id).fg_attempted++;
            
            std::ostringstream desc;
            desc << handler->name << "'s layup is BLOCKED by " << defender->name << "!";
            
            // Resolve block rebound
            if (rng_.uniform() < 0.65f) {
                // Defensive rebound
                std::string reber_id = def_lineup.c.empty() ? def_lineup.pf : def_lineup.c;
                getOrCreateStats(reber_id). rebounds_def++;
                desc << " " << (getPlayerOrNull(reber_id) ? getPlayerOrNull(reber_id)->name : "Defense") << " secures the defensive board.";
                
                state_.possession_team_index = defense_idx;
                state_.shot_clock = 24.0f;
                state_.ball_handler_id = def_lineup.pg.empty() ? def_lineup.sg : def_lineup.pg;
            } else {
                // Offensive retains
                getOrCreateStats(handler_id).rebounds_off++;
                desc << " " << handler->name << " grabs his own blocked shot.";
                state_.shot_clock = 14.0f;
            }
            result.description = desc.str();
            result.possession_ended = true;
            return result;
        }

        // Check for foul on shot
        float foul_chance = 0.08f + (shot_type == ShotType::INSIDE ? 0.12f : 0.0f);
        bool fouled = rng_.uniform() < foul_chance;

        // Calculate shot success probability
        float shooter_rating = 0.5f;
        float base_pct = 0.40f;
        int point_value = 2;

        if (shot_type == ShotType::INSIDE) {
            shooter_rating = handler->ratings.current.inside_scoring;
            base_pct = 0.53f;
        } else if (shot_type == ShotType::THREE_POINT) {
            shooter_rating = handler->ratings.current.three_point;
            base_pct = 0.34f;
            point_value = 3;
        } else {
            shooter_rating = handler->ratings.current.mid_range;
            base_pct = 0.42f;
        }

        float make_prob = base_pct + (shooter_rating - def_rating) * 0.20f;
        if (fouled) make_prob -= 0.10f; // harder to score when fouled
        make_prob = std::clamp(make_prob, 0.15f, 0.85f);

        bool made = rng_.uniform() < make_prob;
        getOrCreateStats(handler_id).fg_attempted++;
        if (shot_type == ShotType::THREE_POINT) {
            getOrCreateStats(handler_id).tp_attempted++;
        }

        std::ostringstream desc;
        std::string shot_name = (shot_type == ShotType::INSIDE ? "layup" : (shot_type == ShotType::THREE_POINT ? "three-pointer" : "mid-range jumper"));
        
        if (fouled) {
            desc << handler->name << " is fouled driving for a " << shot_name << ". ";
            // Increment team fouls
            state_.team_fouls[defense_idx]++;
            if (defender) getOrCreateStats(defender_id).fouls++;

            if (made) {
                // And-one!
                getOrCreateStats(handler_id).fg_made++;
                if (shot_type == ShotType::THREE_POINT) getOrCreateStats(handler_id).tp_made++;
                getOrCreateStats(handler_id).points += point_value;
                state_.score[offense_idx] += point_value;

                desc << "The shot goes in! And-one. ";
                
                // Shoot 1 Free Throw
                getOrCreateStats(handler_id).ft_attempted++;
                if (rng_.uniform() < handler->ratings.current.free_throw) {
                    getOrCreateStats(handler_id).ft_made++;
                    getOrCreateStats(handler_id).points += 1;
                    state_.score[offense_idx] += 1;
                    desc << "Free throw MADE.";
                } else {
                    desc << "Free throw MISSED.";
                }
                
                // Possession swaps after made FT
                state_.possession_team_index = defense_idx;
                state_.shot_clock = 24.0f;
                state_.ball_handler_id = def_lineup.pg.empty() ? def_lineup.sg : def_lineup.pg;
                result.possession_ended = true;
            } else {
                // Missed, shoot 2 or 3 FTs
                int ft_count = (shot_type == ShotType::THREE_POINT ? 3 : 2);
                desc << "Shooting " << ft_count << " free throws. ";
                int ft_made_count = 0;
                for (int i = 0; i < ft_count; ++i) {
                    getOrCreateStats(handler_id).ft_attempted++;
                    if (rng_.uniform() < handler->ratings.current.free_throw) {
                        getOrCreateStats(handler_id).ft_made++;
                        getOrCreateStats(handler_id).points += 1;
                        state_.score[offense_idx] += 1;
                        ft_made_count++;
                    }
                }
                desc << "(" << ft_made_count << "/" << ft_count << " FTs made).";
                
                // Possession swaps after free throws
                state_.possession_team_index = defense_idx;
                state_.shot_clock = 24.0f;
                state_.ball_handler_id = def_lineup.pg.empty() ? def_lineup.sg : def_lineup.pg;
                result.possession_ended = true;
            }
        } else {
            // Normal Shot (No Foul)
            if (made) {
                getOrCreateStats(handler_id).fg_made++;
                if (shot_type == ShotType::THREE_POINT) getOrCreateStats(handler_id).tp_made++;
                getOrCreateStats(handler_id).points += point_value;
                state_.score[offense_idx] += point_value;

                desc << handler->name << " hits a " << shot_name << "!";
                
                state_.possession_team_index = defense_idx;
                state_.shot_clock = 24.0f;
                state_.ball_handler_id = def_lineup.pg.empty() ? def_lineup.sg : def_lineup.pg;
                result.possession_ended = true;
                result.shot_made = true;
                result.points_scored = point_value;
            } else {
                desc << handler->name << " misses the " << shot_name << ".";
                
                // Resolve Rebound
                float off_reb_rating = off_lineup.c.empty() ? 0.5f : getPlayerOrNull(off_lineup.c)->ratings.current.rebounding;
                float def_reb_rating = def_lineup.c.empty() ? 0.5f : getPlayerOrNull(def_lineup.c)->ratings.current.rebounding;
                
                float def_reb_prob = 0.73f + (def_reb_rating - off_reb_rating) * 0.15f;
                def_reb_prob = std::clamp(def_reb_prob, 0.55f, 0.90f);

                if (rng_.uniform() < def_reb_prob) {
                    // Defensive Rebound
                    std::string reber_id = def_lineup.c.empty() ? (def_lineup.pf.empty() ? def_lineup.sf : def_lineup.pf) : def_lineup.c;
                    if (!reber_id.empty()) {
                        getOrCreateStats(reber_id).rebounds_def++;
                        desc << " Defensive rebound by " << (getPlayerOrNull(reber_id) ? getPlayerOrNull(reber_id)->name : "defense") << ".";
                    } else {
                        desc << " Defensive rebound secured.";
                    }
                    
                    state_.possession_team_index = defense_idx;
                    state_.shot_clock = 24.0f;
                    state_.ball_handler_id = def_lineup.pg.empty() ? def_lineup.sg : def_lineup.pg;
                } else {
                    // Offensive Rebound
                    std::string reber_id = off_lineup.c.empty() ? (off_lineup.pf.empty() ? off_lineup.sf : off_lineup.pf) : off_lineup.c;
                    if (!reber_id.empty()) {
                        getOrCreateStats(reber_id).rebounds_off++;
                        desc << " Offensive rebound grabbed by " << (getPlayerOrNull(reber_id) ? getPlayerOrNull(reber_id)->name : "offense") << "!";
                    } else {
                        desc << " Offensive rebound secured!";
                    }
                    
                    state_.shot_clock = 14.0f; // resets to 14s on offensive rebound
                    state_.ball_handler_id = reber_id;
                }
                result.possession_ended = true;
            }
        }

        result.description = desc.str();

    } else {
        // PASS
        std::string receiver_id = selectRandomTeammate(off_lineup, handler_id);
        const Player* receiver = getPlayerOrNull(receiver_id);
        
        std::ostringstream desc;
        if (receiver) {
            desc << handler->name << " passes the ball to " << receiver->name << ".";
            state_.ball_handler_id = receiver_id;
        } else {
            desc << handler->name << " swings the ball around the perimeter.";
        }

        // Shot clock warning
        if (state_.shot_clock <= 3.0f) {
            desc << " (Shot clock ticking down!)";
        }

        result.description = desc.str();
        result.possession_ended = false;
    }

    // Double check shot clock violations
    if (state_.shot_clock <= 0.0f && !result.possession_ended) {
        std::ostringstream desc;
        desc << "Shot clock violation! Turn over on possession.";
        result.description = desc.str();
        result.possession_ended = true;
        
        state_.possession_team_index = defense_idx;
        state_.shot_clock = 24.0f;
        state_.ball_handler_id = def_lineup.pg.empty() ? def_lineup.sg : def_lineup.pg;
    }

    return result;
}
