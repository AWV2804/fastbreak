#include "engine/api/simulation_engine.hpp"

#include <iostream>
#include <unordered_map>
#include <vector>
#include <cassert>

namespace {
Player makePlayer(const std::string& id, const std::string& name, Position pos) {
    Player p;
    p.player_id = id;
    p.name = name;
    p.age = 25;
    p.primary_position = pos;
    p.ratings.current = PlayerRatings{0.6f, 0.6f, 0.6f, 0.7f, 0.6f, 0.6f, 0.6f, 0.6f, 0.6f, 0.6f, 0.6f, 0.6f, 0.6f, 0.6f, 0.6f};
    p.ratings.potential = p.ratings.current;
    return p;
}
}

int main() {
    std::cout << "[SMOKE TEST] Initializing player database..." << std::endl;
    std::unordered_map<std::string, Player> players;

    std::vector<std::string> home_roster;
    std::vector<std::string> away_roster;

    // Starters and bench
    players["h_pg"] = makePlayer("h_pg", "Home PG", Position::PG);
    players["h_sg"] = makePlayer("h_sg", "Home SG", Position::SG);
    players["h_sf"] = makePlayer("h_sf", "Home SF", Position::SF);
    players["h_pf"] = makePlayer("h_pf", "Home PF", Position::PF);
    players["h_c"]  = makePlayer("h_c", "Home C", Position::C);
    players["h_b1"] = makePlayer("h_b1", "Home Bench 1", Position::PG);
    home_roster = {"h_pg", "h_sg", "h_sf", "h_pf", "h_c", "h_b1"};

    players["a_pg"] = makePlayer("a_pg", "Away PG", Position::PG);
    players["a_sg"] = makePlayer("a_sg", "Away SG", Position::SG);
    players["a_sf"] = makePlayer("a_sf", "Away SF", Position::SF);
    players["a_pf"] = makePlayer("a_pf", "Away PF", Position::PF);
    players["a_c"]  = makePlayer("a_c", "Away C", Position::C);
    players["a_b1"] = makePlayer("a_b1", "Away Bench 1", Position::C);
    away_roster = {"a_pg", "a_sg", "a_sf", "a_pf", "a_c", "a_b1"};

    CourtLineup home_lineup = {"h_pg", "h_sg", "h_sf", "h_pf", "h_c"};
    CourtLineup away_lineup = {"a_pg", "a_sg", "a_sf", "a_pf", "a_c"};

    LeagueConfig config;
    SimulationEngine engine(config, players);

    std::cout << "[SMOKE TEST] Starting game..." << std::endl;
    engine.startGame("SMOKE-1", home_roster, away_roster, home_lineup, away_lineup, 42U);

    GameSnapshotDTO snap = engine.getGameSnapshot();
    assert(snap.game_id == "SMOKE-1");
    assert(snap.home_score == 0);
    assert(snap.away_score == 0);
    assert(snap.quarter == 1);
    assert(!snap.game_over);

    std::cout << "[SMOKE TEST] Simulating game actions..." << std::endl;
    int actions = 0;
    while (engine.advanceToNextDecisionPoint() && actions < 1000) {
        actions++;
    }

    std::cout << "[SMOKE TEST] Simulated " << actions << " actions." << std::endl;
    GameSnapshotDTO final_snap = engine.getGameSnapshot();
    std::cout << "[SMOKE TEST] Final Score: Home " << final_snap.home_score << " - Away " << final_snap.away_score << std::endl;
    std::cout << "[SMOKE TEST] Quarters played: " << final_snap.quarter << std::endl;
    std::cout << "[SMOKE TEST] Game Over flag: " << (final_snap.game_over ? "true" : "false") << std::endl;

    std::cout << "[SMOKE TEST] PASS!" << std::endl;
    return 0;
}
