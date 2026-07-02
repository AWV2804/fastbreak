#include "engine/api/json.hpp"
#include "engine/api/simulation_engine.hpp"

#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

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

}  // namespace

int main() {
    LeagueConfig config;
    std::unordered_map<std::string, Player> players;

    // Create Home roster
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

    // Create Away roster
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
    bool started = false;

    std::string line;
    while (std::getline(std::cin, line)) {
        if (line == "quit") {
            std::cout << "{\"ok\":true,\"type\":\"quit\"}" << std::endl;
            break;
        }

        if (line == "init") {
            engine.startGame("FASTBREAK-UNITY-1", home_roster, away_roster, home_lineup, away_lineup, 1337U);
            started = true;
            std::cout << "{\"ok\":true,\"type\":\"snapshot\",\"snapshot\":" << toJson(engine.getGameSnapshot()) << "}" << std::endl;
            continue;
        }

        if (!started) {
            writeError("Game not initialized. Send 'init' first.");
            continue;
        }

        if (line == "snapshot") {
            std::cout << "{\"ok\":true,\"type\":\"snapshot\",\"snapshot\":" << toJson(engine.getGameSnapshot()) << "}" << std::endl;
            continue;
        }

        if (line == "step") {
            bool running = engine.advanceToNextDecisionPoint();
            std::cout << "{\"ok\":true,\"type\":\"step\",\"running\":" << (running ? "true" : "false")
                      << ",\"snapshot\":" << toJson(engine.getGameSnapshot()) << "}" << std::endl;
            continue;
        }

        if (line.rfind("command ", 0) == 0) {
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
            int team_idx = snap.decision_point.offense_team_index; // default to active offense team
            
            CommandValidation validation = engine.submitManagerCommand(team_idx, cmd);
            std::cout << "{\"ok\":true,\"type\":\"command\",\"command\":" << toJson(validation) << "}" << std::endl;
            continue;
        }

        writeError("Unknown message: " + line);
    }

    return 0;
}
