#include "engine/game/game_simulator.hpp"
#include "engine/sim/possession_resolver.hpp"

#include <algorithm>
#include <sstream>

GameSimulator::GameSimulator(const std::unordered_map<std::string, Player>& players)
    : players_(players), rng_(42U) {}

GameState GameSimulator::startGame(const std::string& game_id,
                                   const std::vector<std::string>& home_roster,
                                   const std::vector<std::string>& away_roster,
                                   const CourtLineup& home_lineup,
                                   const CourtLineup& away_lineup,
                                   std::uint_fast32_t seed) {
    rng_ = RNG(seed);
    state_ = GameState{};
    state_.game_id = game_id;
    state_.seed = seed;
    state_.quarter = 1;
    state_.game_clock = 720.0f; // 12-minute NBA quarter
    state_.shot_clock = 24.0f;
    state_.score = {0, 0};
    state_.team_fouls = {0, 0};
    state_.timeouts_remaining = {7, 7};
    
    state_.rosters[0] = home_roster;
    state_.rosters[1] = away_roster;
    state_.lineups[0] = home_lineup;
    state_.lineups[1] = away_lineup;

    state_.player_stats.clear();
    state_.events.clear();
    state_.game_over = false;

    logEvent("--- Game Started ---");
    triggerJumpBall();

    return state_;
}

void GameSimulator::logEvent(const std::string& description) {
    GameEventLog event;
    event.quarter = state_.quarter;
    event.clock = state_.game_clock;
    event.description = description;
    state_.events.push_back(event);
}

void GameSimulator::triggerJumpBall() {
    std::string home_c_id = state_.lineups[0].c;
    std::string away_c_id = state_.lineups[1].c;

    auto home_c = players_.find(home_c_id);
    auto away_c = players_.find(away_c_id);

    float home_chance = 0.5f;
    if (home_c != players_.end() && away_c != players_.end()) {
        float home_val = home_c->second.ratings.current.rebounding + (home_c->second.ratings.current.strength * 0.5f);
        float away_val = away_c->second.ratings.current.rebounding + (away_c->second.ratings.current.strength * 0.5f);
        home_chance = home_val / (home_val + away_val);
    }

    std::string home_name = (home_c != players_.end()) ? home_c->second.name : "Home Center";
    std::string away_name = (away_c != players_.end()) ? away_c->second.name : "Away Center";

    std::ostringstream ss;
    ss << "Jump ball between " << home_name << " and " << away_name << "... ";

    if (rng_.uniform() < home_chance) {
        state_.possession_team_index = 0;
        state_.ball_handler_id = state_.lineups[0].pg;
        ss << home_name << " wins the tip.";
    } else {
        state_.possession_team_index = 1;
        state_.ball_handler_id = state_.lineups[1].pg;
        ss << away_name << " wins the tip.";
    }

    logEvent(ss.str());
}

CommandValidation GameSimulator::submitManagerCommand(int team_index, const ManagerCommand& command) {
    CommandValidation validation;
    if (team_index < 0 || team_index > 1) {
        validation.accepted = false;
        validation.reason = "Invalid team index.";
        return validation;
    }

    if (command.type == ManagerCommandType::TIMEOUT) {
        if (state_.timeouts_remaining[team_index] <= 0) {
            validation.accepted = false;
            validation.reason = "No timeouts remaining.";
            return validation;
        }
        state_.timeouts_remaining[team_index]--;
        logEvent((team_index == 0 ? "Home" : "Away") + std::string(" team calls timeout."));
        
        // Recover stamina slightly for all court players on this team
        auto& lineup = state_.lineups[team_index];
        std::vector<std::string> court_ids = {lineup.pg, lineup.sg, lineup.sf, lineup.pf, lineup.c};
        for (const auto& pid : court_ids) {
            auto it = players_.find(pid);
            if (it != players_.end()) {
                it->second.fatigue = std::max(0.0f, it->second.fatigue - 0.08f);
            }
        }
        // Rest shot clock to 14 or 24 depending on state
        state_.shot_clock = std::max(state_.shot_clock, 14.0f);
        validation.accepted = true;
        return validation;
    }

    if (command.type == ManagerCommandType::SUBSTITUTION) {
        if (!command.payload.has_value()) {
            validation.accepted = false;
            validation.reason = "Missing substitution payload.";
            return validation;
        }

        // Parse: out_id:in_id:position
        std::string payload = command.payload.value();
        std::stringstream ss(payload);
        std::string out_id, in_id, pos_str;
        std::getline(ss, out_id, ':');
        std::getline(ss, in_id, ':');
        std::getline(ss, pos_str, ':');

        Position pos = Position::PG;
        if (pos_str == "SG") pos = Position::SG;
        else if (pos_str == "SF") pos = Position::SF;
        else if (pos_str == "PF") pos = Position::PF;
        else if (pos_str == "C") pos = Position::C;

        // Verify out_id is on court
        CourtLineup& lineup = state_.lineups[team_index];
        if (!lineup.has_player(out_id)) {
            validation.accepted = false;
            validation.reason = "Player to replace is not on court.";
            return validation;
        }

        // Verify in_id is in roster and not already on court
        const auto& roster = state_.rosters[team_index];
        if (std::find(roster.begin(), roster.end(), in_id) == roster.end()) {
            validation.accepted = false;
            validation.reason = "Incoming player is not on the roster.";
            return validation;
        }

        if (lineup.has_player(in_id)) {
            validation.accepted = false;
            validation.reason = "Incoming player is already on the court.";
            return validation;
        }

        // Perform substitution
        lineup.set_player_at_position(pos, in_id);

        auto p_out = players_.find(out_id);
        auto p_in = players_.find(in_id);
        std::string out_name = (p_out != players_.end()) ? p_out->second.name : out_id;
        std::string in_name = (p_in != players_.end()) ? p_in->second.name : in_id;

        std::ostringstream log_ss;
        log_ss << "SUB: " << in_name << " enters the game, replacing " << out_name;
        logEvent(log_ss.str());

        // If subbed out player was the ball handler, transfer handler to incoming player
        if (state_.ball_handler_id == out_id) {
            state_.ball_handler_id = in_id;
        }

        validation.accepted = true;
        return validation;
    }

    if (command.type == ManagerCommandType::SET_PACE) {
        if (!command.payload.has_value()) {
            validation.accepted = false;
            validation.reason = "Missing pace payload.";
            return validation;
        }
        std::string pace = command.payload.value();
        if (pace == "slow") state_.strategies[team_index].pace = StrategyPace::SLOW;
        else if (pace == "fast") state_.strategies[team_index].pace = StrategyPace::FAST;
        else state_.strategies[team_index].pace = StrategyPace::NEUTRAL;
        
        validation.accepted = true;
        return validation;
    }

    if (command.type == ManagerCommandType::SET_OFFENSE) {
        if (!command.payload.has_value()) {
            validation.accepted = false;
            validation.reason = "Missing offense strategy payload.";
            return validation;
        }
        std::string off = command.payload.value();
        if (off == "inside") state_.strategies[team_index].offense = StrategyOffense::INSIDE;
        else if (off == "perimeter") state_.strategies[team_index].offense = StrategyOffense::PERIMETER;
        else state_.strategies[team_index].offense = StrategyOffense::BALANCED;

        validation.accepted = true;
        return validation;
    }

    if (command.type == ManagerCommandType::SET_DEFENSE) {
        if (!command.payload.has_value()) {
            validation.accepted = false;
            validation.reason = "Missing defense strategy payload.";
            return validation;
        }
        std::string def = command.payload.value();
        if (def == "zone") state_.strategies[team_index].defense = StrategyDefense::ZONE;
        else if (def == "press") state_.strategies[team_index].defense = StrategyDefense::PRESS;
        else state_.strategies[team_index].defense = StrategyDefense::MAN_TO_MAN;

        validation.accepted = true;
        return validation;
    }

    validation.accepted = false;
    validation.reason = "Unknown command type.";
    return validation;
}

bool GameSimulator::advanceToNextDecisionPoint() {
    if (state_.game_over) {
        return false;
    }

    state_.decision_point.reset();

    float start_time = state_.game_clock;

    // Run action
    PossessionResolver resolver(state_, players_, rng_);
    PlayActionResult result = resolver.resolveNextAction();
    
    float elapsed_time = start_time - state_.game_clock;
    if (elapsed_time > 0.0f) {
        // Log player minutes and increase fatigue
        for (int team = 0; team < 2; ++team) {
            const auto& lineup = state_.lineups[team];
            std::vector<std::string> court_ids = {lineup.pg, lineup.sg, lineup.sf, lineup.pf, lineup.c};
            for (const auto& pid : court_ids) {
                if (pid.empty()) continue;
                
                // Track minutes
                auto it_stats = state_.player_stats.find(pid);
                if (it_stats != state_.player_stats.end()) {
                    it_stats->second.minutes_played += elapsed_time / 60.0f;
                } else {
                    PlayerGameStats new_stats;
                    new_stats.player_id = pid;
                    new_stats.minutes_played = elapsed_time / 60.0f;
                    state_.player_stats[pid] = new_stats;
                }

                // Increase fatigue
                auto it_p = players_.find(pid);
                if (it_p != players_.end()) {
                    float stamina = it_p->second.ratings.current.stamina;
                    // Standard fatigue rate: higher stamina = slower fatigue
                    it_p->second.fatigue += elapsed_time / (1200.0f * (0.5f + stamina));
                    it_p->second.fatigue = std::clamp(it_p->second.fatigue, 0.0f, 1.0f);
                }
            }
        }
    }

    if (!result.description.empty()) {
        logEvent(result.description);
    }

    // Check for quarter end or overtime
    if (state_.game_clock <= 0.0f) {
        processQuarterEnd();
    }

    // Check if decision point should be set (e.g. at end of quarter or game over)
    if (state_.game_over) {
        DecisionPoint dp;
        dp.awaiting_command = false; // No decisions left to make
        dp.offense_team_index = 0;
        dp.defense_team_index = 1;
        state_.decision_point = dp;
    } else if (state_.game_clock == 720.0f || state_.game_clock == 300.0f) { // Start of new quarter/OT
        DecisionPoint dp;
        dp.awaiting_command = true;
        dp.offense_team_index = state_.possession_team_index;
        dp.defense_team_index = 1 - state_.possession_team_index;
        state_.decision_point = dp;
    }

    return !state_.game_over;
}

void GameSimulator::processQuarterEnd() {
    if (state_.quarter < 4) {
        state_.quarter++;
        state_.game_clock = 720.0f;
        state_.shot_clock = 24.0f;
        state_.team_fouls = {0, 0};
        
        std::ostringstream ss;
        ss << "End of Quarter " << (state_.quarter - 1) << ". Moving to Quarter " << state_.quarter << ". Score: Home " << state_.score[0] << ", Away " << state_.score[1];
        logEvent(ss.str());
        
        // Reset possession for quarter start
        triggerJumpBall();
    } else {
        // Quarter 4 or OT
        if (state_.score[0] == state_.score[1]) {
            // Overtime!
            state_.quarter++;
            state_.game_clock = 300.0f; // 5-minute OT
            state_.shot_clock = 24.0f;
            state_.team_fouls = {0, 0};
            
            std::ostringstream ss;
            ss << "Tied game at " << state_.score[0] << "! Moving to Overtime " << (state_.quarter - 4) << ".";
            logEvent(ss.str());
            
            triggerJumpBall();
        } else {
            // Game Over!
            state_.game_over = true;
            std::ostringstream ss;
            ss << "--- Game Over! Final Score: Home " << state_.score[0] << " - Away " << state_.score[1] << " ---";
            logEvent(ss.str());
        }
    }
}
