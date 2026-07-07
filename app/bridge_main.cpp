#include "engine/api/json.hpp"
#include "engine/api/simulation_engine.hpp"
#include "engine/franchise/franchise_engine.hpp"

#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include <sstream>

namespace {

Player makePlayer(const std::string& id, const std::string& name, Position pos, float shooting, float defense, float rebounding) {
    Player p;
    p.player_id = id;
    p.name = name;
    p.age = 26;
    p.primary_position = pos;
    
    // Set customized ratings
    p.ratings.current.inside_scoring = shooting * 0.9f;
    p.ratings.current.mid_range = shooting;
    p.ratings.current.three_point = (pos == Position::C || pos == Position::PF) ? shooting * 0.6f : shooting * 1.1f;
    p.ratings.current.free_throw = 0.75f;
    
    p.ratings.current.passing = 0.5f + (pos == Position::PG ? 0.3f : 0.0f);
    p.ratings.current.dribbling = 0.5f + (pos == Position::PG ? 0.3f : 0.0f);
    p.ratings.current.offensive_iq = 0.6f;
    
    p.ratings.current.perimeter_defense = (pos == Position::PG || pos == Position::SG) ? defense : defense * 0.7f;
    p.ratings.current.interior_defense = (pos == Position::C || pos == Position::PF) ? defense : defense * 0.5f;
    p.ratings.current.stealing = defense * 0.8f;
    p.ratings.current.blocking = (pos == Position::C || pos == Position::PF) ? defense * 0.9f : defense * 0.2f;
    
    p.ratings.current.rebounding = rebounding;
    p.ratings.current.speed = 0.6f;
    p.ratings.current.strength = 0.6f;
    p.ratings.current.stamina = 0.7f;
    
    p.ratings.potential = p.ratings.current;
    return p;
}

ManagerCommandType parseCommandType(const std::string& cmd, std::string& payload) {
    if (cmd == "timeout") {
        return ManagerCommandType::TIMEOUT;
    }
    if (cmd.rfind("sub ", 0) == 0) {
        payload = cmd.substr(4);
        return ManagerCommandType::SUBSTITUTION;
    }
    if (cmd.rfind("pace ", 0) == 0) {
        payload = cmd.substr(5);
        return ManagerCommandType::SET_PACE;
    }
    if (cmd.rfind("focus ", 0) == 0) {
        payload = cmd.substr(6);
        return ManagerCommandType::SET_OFFENSE;
    }
    if (cmd.rfind("defense ", 0) == 0) {
        payload = cmd.substr(8);
        return ManagerCommandType::SET_DEFENSE;
    }
    return ManagerCommandType::NONE;
}

void writeError(const std::string& message) {
    std::cout << "{\"ok\":false,\"type\":\"error\",\"message\":\"" << message << "\"}" << std::endl;
}

std::vector<std::string> splitString(const std::string& s, char delim) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(s);
    while (std::getline(tokenStream, token, delim)) {
        if (!token.empty()) {
            tokens.push_back(token);
        }
    }
    return tokens;
}

void registerDefaultFranchise(FranchiseEngine& eng) {
    auto registerTeam = [&](const std::string& id, const std::string& name, float skill) {
        std::vector<std::string> roster;
        CourtLineup lineup;

        std::string pg_id = id + "_pg";
        std::string sg_id = id + "_sg";
        std::string sf_id = id + "_sf";
        std::string pf_id = id + "_pf";
        std::string c_id  = id + "_c";

        eng.registerPlayer(makePlayer(pg_id, name + " PG", Position::PG, skill, skill, 0.4f));
        eng.registerPlayer(makePlayer(sg_id, name + " SG", Position::SG, skill, skill, 0.5f));
        eng.registerPlayer(makePlayer(sf_id, name + " SF", Position::SF, skill, skill, 0.6f));
        eng.registerPlayer(makePlayer(pf_id, name + " PF", Position::PF, skill, skill, 0.7f));
        eng.registerPlayer(makePlayer(c_id, name + " C", Position::C, skill, skill, 0.8f));

        roster = {pg_id, sg_id, sf_id, pf_id, c_id};
        lineup.pg = pg_id;
        lineup.sg = sg_id;
        lineup.sf = sf_id;
        lineup.pf = pf_id;
        lineup.c = c_id;

        eng.addTeam(id, name, lineup, roster);
    };

    registerTeam("BOS", "Boston Celtics", 0.72f);
    registerTeam("MIA", "Miami Heat", 0.62f);
    registerTeam("LAL", "Los Angeles Lakers", 0.60f);
    registerTeam("DAL", "Dallas Mavericks", 0.65f);
}

}  // namespace

int main() {
    LeagueConfig config;
    config.roster_limit = 15;
    
    std::unordered_map<std::string, Player> players;

    // Create Home roster for single game mode
    players["h_pg"] = makePlayer("h_pg", "Marcus Smart", Position::PG, 0.55f, 0.75f, 0.40f);
    players["h_sg"] = makePlayer("h_sg", "Jaylen Brown", Position::SG, 0.72f, 0.68f, 0.55f);
    players["h_sf"] = makePlayer("h_sf", "Jayson Tatum", Position::SF, 0.78f, 0.65f, 0.65f);
    players["h_pf"] = makePlayer("h_pf", "Al Horford", Position::PF, 0.60f, 0.68f, 0.70f);
    players["h_c"]  = makePlayer("h_c", "Kristaps Porzingis", Position::C, 0.70f, 0.75f, 0.75f);
    players["h_b1"] = makePlayer("h_b1", "Derrick White", Position::PG, 0.65f, 0.72f, 0.45f);
    players["h_b2"] = makePlayer("h_b2", "Sam Hauser", Position::SF, 0.62f, 0.50f, 0.45f);

    std::vector<std::string> home_roster = {"h_pg", "h_sg", "h_sf", "h_pf", "h_c", "h_b1", "h_b2"};
    CourtLineup home_lineup;
    home_lineup.pg = "h_pg";
    home_lineup.sg = "h_sg";
    home_lineup.sf = "h_sf";
    home_lineup.pf = "h_pf";
    home_lineup.c = "h_c";

    // Create Away roster for single game mode
    players["a_pg"] = makePlayer("a_pg", "Kyrie Irving", Position::PG, 0.82f, 0.45f, 0.35f);
    players["a_sg"] = makePlayer("a_sg", "Luka Doncic", Position::SG, 0.88f, 0.50f, 0.70f);
    players["a_sf"] = makePlayer("a_sf", "Derrick Jones Jr.", Position::SF, 0.50f, 0.70f, 0.50f);
    players["a_pf"] = makePlayer("a_pf", "P.J. Washington", Position::PF, 0.58f, 0.62f, 0.60f);
    players["a_c"]  = makePlayer("a_c", "Daniel Gafford", Position::C, 0.50f, 0.68f, 0.80f);
    players["a_b1"] = makePlayer("a_b1", "Dereck Lively II", Position::C, 0.55f, 0.70f, 0.75f);
    players["a_b2"] = makePlayer("a_b2", "Tim Hardaway Jr.", Position::SG, 0.60f, 0.40f, 0.35f);

    std::vector<std::string> away_roster = {"a_pg", "a_sg", "a_sf", "a_pf", "a_c", "a_b1", "a_b2"};
    CourtLineup away_lineup;
    away_lineup.pg = "a_pg";
    away_lineup.sg = "a_sg";
    away_lineup.sf = "a_sf";
    away_lineup.pf = "a_pf";
    away_lineup.c = "a_c";

    SimulationEngine engine(config, players);
    std::unique_ptr<FranchiseEngine> franchise_eng;
    bool started = false;

    std::string line;
    while (std::getline(std::cin, line)) {
        if (line == "quit") {
            std::cout << "{\"ok\":true,\"type\":\"quit\"}" << std::endl;
            break;
        }

        // --- SINGLE GAME ENDPOINTS ---
        if (line == "init") {
            engine.startGame("FASTBREAK-UNITY-1", home_roster, away_roster, home_lineup, away_lineup, 1337U);
            started = true;
            std::cout << "{\"ok\":true,\"type\":\"snapshot\",\"snapshot\":" << toJson(engine.getGameSnapshot()) << "}" << std::endl;
            continue;
        }

        if (line == "snapshot") {
            if (!started) {
                writeError("Game not initialized. Send 'init' first.");
                continue;
            }
            std::cout << "{\"ok\":true,\"type\":\"snapshot\",\"snapshot\":" << toJson(engine.getGameSnapshot()) << "}" << std::endl;
            continue;
        }

        if (line == "step") {
            if (!started) {
                writeError("Game not initialized. Send 'init' first.");
                continue;
            }
            bool running = engine.advanceToNextDecisionPoint();
            std::cout << "{\"ok\":true,\"type\":\"step\",\"running\":" << (running ? "true" : "false")
                      << ",\"snapshot\":" << toJson(engine.getGameSnapshot()) << "}" << std::endl;
            continue;
        }

        if (line.rfind("command ", 0) == 0) {
            if (!started) {
                writeError("Game not initialized. Send 'init' first.");
                continue;
            }
            std::string cmd_text = line.substr(8);
            std::string payload = "";
            ManagerCommandType type = parseCommandType(cmd_text, payload);
            if (type == ManagerCommandType::NONE) {
                writeError("Unknown command");
                continue;
            }

            ManagerCommand cmd;
            cmd.type = type;
            if (!payload.empty()) {
                cmd.payload = payload;
            }

            GameSnapshotDTO snap = engine.getGameSnapshot();
            int team_idx = snap.decision_point.offense_team_index;
            
            CommandValidation validation = engine.submitManagerCommand(team_idx, cmd);
            std::cout << "{\"ok\":true,\"type\":\"command\",\"command\":" << toJson(validation) << "}" << std::endl;
            continue;
        }

        // --- FRANCHISE FRANCHISE ENDPOINTS ---
        if (line == "franchise_init") {
            franchise_eng = std::make_unique<FranchiseEngine>(config, 42U);
            registerDefaultFranchise(*franchise_eng);
            franchise_eng->generateSchedule();
            std::cout << "{\"ok\":true,\"type\":\"franchise_init\",\"current_day\":" 
                      << franchise_eng->currentDay() << "}" << std::endl;
            continue;
        }

        if (line.rfind("franchise_simulate_day ", 0) == 0) {
            if (!franchise_eng) {
                writeError("Franchise not initialized.");
                continue;
            }
            std::string player_team = line.substr(23);
            std::vector<std::string> logs;
            franchise_eng->simulateDay(player_team, logs);
            std::cout << "{\"ok\":true,\"type\":\"franchise_simulate_day\",\"current_day\":" 
                      << franchise_eng->currentDay() << ",\"logs\":[";
            for (size_t i = 0; i < logs.size(); ++i) {
                std::cout << "\"" << logs[i] << "\"";
                if (i + 1 < logs.size()) std::cout << ",";
            }
            std::cout << "]}" << std::endl;
            continue;
        }

        if (line == "franchise_standings") {
            if (!franchise_eng) {
                writeError("Franchise not initialized.");
                continue;
            }
            auto standings = franchise_eng->getStandings();
            std::cout << "{\"ok\":true,\"type\":\"franchise_standings\",\"standings\":[";
            for (size_t i = 0; i < standings.size(); ++i) {
                std::cout << toJson(standings[i]);
                if (i + 1 < standings.size()) std::cout << ",";
            }
            std::cout << "]}" << std::endl;
            continue;
        }

        if (line == "franchise_schedule") {
            if (!franchise_eng) {
                writeError("Franchise not initialized.");
                continue;
            }
            const auto& schedule = franchise_eng->schedule();
            std::cout << "{\"ok\":true,\"type\":\"franchise_schedule\",\"schedule\":[";
            for (size_t i = 0; i < schedule.size(); ++i) {
                std::cout << toJson(schedule[i]);
                if (i + 1 < schedule.size()) std::cout << ",";
            }
            std::cout << "]}" << std::endl;
            continue;
        }

        if (line.rfind("franchise_trade_evaluate ", 0) == 0) {
            if (!franchise_eng) {
                writeError("Franchise not initialized.");
                continue;
            }
            std::string args = line.substr(25);
            std::vector<std::string> tokens = splitString(args, ' ');
            if (tokens.size() < 4) {
                writeError("Invalid arguments. Expected: team_a team_b sent_pids recv_pids");
                continue;
            }
            TradeProposal proposal;
            proposal.team_a_id = tokens[0];
            proposal.team_b_id = tokens[1];
            proposal.assets_sent = splitString(tokens[2], ',');
            proposal.assets_received = splitString(tokens[3], ',');
            
            TradeResult result = franchise_eng->evaluateTrade(proposal);
            std::cout << "{\"ok\":true,\"type\":\"franchise_trade_evaluate\",\"result\":" 
                      << toJson(result) << "}" << std::endl;
            continue;
        }

        if (line.rfind("franchise_trade_execute ", 0) == 0) {
            if (!franchise_eng) {
                writeError("Franchise not initialized.");
                continue;
            }
            std::string args = line.substr(24);
            std::vector<std::string> tokens = splitString(args, ' ');
            if (tokens.size() < 4) {
                writeError("Invalid arguments. Expected: team_a team_b sent_pids recv_pids");
                continue;
            }
            TradeProposal proposal;
            proposal.team_a_id = tokens[0];
            proposal.team_b_id = tokens[1];
            proposal.assets_sent = splitString(tokens[2], ',');
            proposal.assets_received = splitString(tokens[3], ',');
            
            bool success = franchise_eng->executeTrade(proposal);
            std::cout << "{\"ok\":true,\"type\":\"franchise_trade_execute\",\"success\":" 
                      << (success ? "true" : "false") << "}" << std::endl;
            continue;
        }

        if (line == "franchise_draft_class") {
            if (!franchise_eng) {
                writeError("Franchise not initialized.");
                continue;
            }
            if (franchise_eng->getDraftClass().empty()) {
                franchise_eng->generateDraftClass();
            }
            const auto& prospects = franchise_eng->getDraftClass();
            std::cout << "{\"ok\":true,\"type\":\"franchise_draft_class\",\"prospects\":[";
            for (size_t i = 0; i < prospects.size(); ++i) {
                std::cout << toJson(prospects[i]);
                if (i + 1 < prospects.size()) std::cout << ",";
            }
            std::cout << "]}" << std::endl;
            continue;
        }

        if (line == "franchise_draft_order") {
            if (!franchise_eng) {
                writeError("Franchise not initialized.");
                continue;
            }
            std::vector<std::string> order = franchise_eng->getDraftOrder();
            std::cout << "{\"ok\":true,\"type\":\"franchise_draft_order\",\"order\":[";
            for (size_t i = 0; i < order.size(); ++i) {
                std::cout << "\"" << order[i] << "\"";
                if (i + 1 < order.size()) std::cout << ",";
            }
            std::cout << "]}" << std::endl;
            continue;
        }

        if (line.rfind("franchise_draft_pick ", 0) == 0) {
            if (!franchise_eng) {
                writeError("Franchise not initialized.");
                continue;
            }
            std::string args = line.substr(21);
            std::vector<std::string> tokens = splitString(args, ' ');
            if (tokens.size() < 3) {
                writeError("Invalid arguments. Expected: team_id prospect_id pick_type(player/ai)");
                continue;
            }
            std::string team_id = tokens[0];
            std::string prospect_id = tokens[1];
            std::string pick_type = tokens[2];
            
            std::string drafted_name = "";
            bool success = false;
            if (pick_type == "player") {
                success = franchise_eng->executePlayerDraftPick(team_id, prospect_id);
                if (success) {
                    drafted_name = "Drafted Pick";
                }
            } else {
                drafted_name = franchise_eng->executeAIDraftPick(team_id);
                success = !drafted_name.empty();
            }
            std::cout << "{\"ok\":true,\"type\":\"franchise_draft_pick\",\"success\":" 
                      << (success ? "true" : "false") << ",\"drafted_name\":\"" << drafted_name << "\"}" << std::endl;
            continue;
        }

        writeError("Unknown message: " + line);
    }

    return 0;
}
