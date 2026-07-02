#include "engine/persistence/sqlite_store.hpp"

#include <iostream>
#include <vector>
#include <unordered_map>
#include <cstdio>

namespace {
Player makeDummyPlayer(const std::string& id, const std::string& name) {
    Player p;
    p.player_id = id;
    p.name = name;
    p.age = 22;
    p.primary_position = Position::SG;
    
    // Ratings
    p.ratings.current.inside_scoring = 0.65f;
    p.ratings.current.mid_range = 0.70f;
    p.ratings.current.three_point = 0.75f;
    p.ratings.current.free_throw = 0.80f;
    p.ratings.current.passing = 0.60f;
    p.ratings.current.dribbling = 0.62f;
    p.ratings.current.offensive_iq = 0.68f;
    p.ratings.current.perimeter_defense = 0.58f;
    p.ratings.current.interior_defense = 0.40f;
    p.ratings.current.stealing = 0.55f;
    p.ratings.current.blocking = 0.30f;
    p.ratings.current.rebounding = 0.45f;
    p.ratings.current.speed = 0.72f;
    p.ratings.current.strength = 0.50f;
    p.ratings.current.stamina = 0.70f;
    
    p.ratings.potential = p.ratings.current;
    p.fatigue = 0.15f;
    return p;
}
}

int main() {
    const std::string db_path = "test_persistence.db";
    
    // Ensure clean state
    std::remove(db_path.c_str());

    std::cout << "[PERSISTENCE TEST] Creating test save bundle..." << std::endl;
    SaveBundle original_bundle;
    original_bundle.save_id = "SAVE-GAME-999";
    
    // 1. Players
    original_bundle.players.push_back(makeDummyPlayer("p1", "Luka Doncic"));
    original_bundle.players.push_back(makeDummyPlayer("p2", "Kyrie Irving"));

    // 2. Rosters
    TeamRoster r;
    r.team_id = "DAL";
    r.roster_player_ids = {"p1", "p2"};
    original_bundle.rosters.push_back(r);

    // 3. Games
    GameState game;
    game.game_id = "DAL-MIA-101";
    game.quarter = 2;
    game.game_clock = 450.5f;
    game.shot_clock = 18.2f;
    game.score = {45, 42};
    game.team_fouls = {3, 4};
    game.timeouts_remaining = {5, 6};
    game.possession_team_index = 1;
    game.ball_handler_id = "p2";
    game.game_over = false;
    game.seed = 9999U;
    
    game.lineups[0].pg = "p1";
    game.lineups[0].sg = "p2";
    game.lineups[1].pg = "opp1";
    game.lineups[1].sg = "opp2";

    game.rosters[0] = {"p1", "p2"};
    game.rosters[1] = {"opp1", "opp2"};

    game.strategies[0].pace = StrategyPace::FAST;
    game.strategies[0].offense = StrategyOffense::PERIMETER;
    game.strategies[0].defense = StrategyDefense::PRESS;

    GameEventLog ev1;
    ev1.quarter = 1;
    ev1.clock = 710.0f;
    ev1.description = "Jump ball won by DAL.";
    game.events.push_back(ev1);

    GameEventLog ev2;
    ev2.quarter = 2;
    ev2.clock = 460.0f;
    ev2.description = "Luka Doncic hits a stepback three-pointer!";
    game.events.push_back(ev2);

    original_bundle.games.push_back(game);

    // 4. Save Database (Scoped to close the store handle)
    {
        std::cout << "[PERSISTENCE TEST] Opening database for saving..." << std::endl;
        SqliteStore store(db_path);

        std::cout << "[PERSISTENCE TEST] Running migrations..." << std::endl;
        if (!store.migrate()) {
            std::cerr << "ERROR: Migration failed!" << std::endl;
            return 1;
        }

        std::cout << "[PERSISTENCE TEST] Saving bundle..." << std::endl;
        if (!store.saveBundle(original_bundle, "overwrite")) {
            std::cerr << "ERROR: Save failed!" << std::endl;
            return 1;
        }
        std::cout << "[PERSISTENCE TEST] Database saved and closed successfully." << std::endl;
    }

    // 5. Load Database (Scoped to close the store handle)
    SaveBundle loaded_bundle;
    {
        std::cout << "[PERSISTENCE TEST] Opening database for loading..." << std::endl;
        SqliteStore store(db_path);

        std::cout << "[PERSISTENCE TEST] Loading bundle back..." << std::endl;
        loaded_bundle = store.loadBundle("SAVE-GAME-999");
        std::cout << "[PERSISTENCE TEST] Database loaded and closed successfully." << std::endl;
    }

    // 6. Verify data
    std::cout << "[PERSISTENCE TEST] Verifying loaded data..." << std::endl;
    if (loaded_bundle.save_id != original_bundle.save_id) {
        std::cerr << "ERROR: save_id mismatch! Expected " << original_bundle.save_id << ", got " << loaded_bundle.save_id << std::endl;
        return 1;
    }
    
    // Verify Players
    if (loaded_bundle.players.size() != 2) {
        std::cerr << "ERROR: Player count mismatch! Expected 2, got " << loaded_bundle.players.size() << std::endl;
        return 1;
    }
    if (loaded_bundle.players[0].player_id != "p1") {
        std::cerr << "ERROR: Player ID mismatch!" << std::endl;
        return 1;
    }
    if (loaded_bundle.players[0].name != "Luka Doncic") {
        std::cerr << "ERROR: Player name mismatch!" << std::endl;
        return 1;
    }
    if (loaded_bundle.players[0].ratings.current.three_point != 0.75f) {
        std::cerr << "ERROR: Player rating mismatch!" << std::endl;
        return 1;
    }

    // Verify Rosters
    if (loaded_bundle.rosters.size() != 1) {
        std::cerr << "ERROR: Roster count mismatch!" << std::endl;
        return 1;
    }
    if (loaded_bundle.rosters[0].team_id != "DAL") {
        std::cerr << "ERROR: Roster team ID mismatch!" << std::endl;
        return 1;
    }
    if (loaded_bundle.rosters[0].roster_player_ids.size() != 2) {
        std::cerr << "ERROR: Roster player count mismatch!" << std::endl;
        return 1;
    }

    // Verify GameState
    if (loaded_bundle.games.size() != 1) {
        std::cerr << "ERROR: Game count mismatch!" << std::endl;
        return 1;
    }
    const GameState& loaded_game = loaded_bundle.games[0];
    if (loaded_game.game_id != "DAL-MIA-101") {
        std::cerr << "ERROR: Game ID mismatch!" << std::endl;
        return 1;
    }
    if (loaded_game.quarter != 2) {
        std::cerr << "ERROR: Game quarter mismatch!" << std::endl;
        return 1;
    }
    if (loaded_game.score[0] != 45 || loaded_game.score[1] != 42) {
        std::cerr << "ERROR: Game score mismatch!" << std::endl;
        return 1;
    }
    if (loaded_game.lineups[0].pg != "p1" || loaded_game.lineups[0].sg != "p2") {
        std::cerr << "ERROR: Game lineups mismatch!" << std::endl;
        return 1;
    }
    if (loaded_game.strategies[0].pace != StrategyPace::FAST) {
        std::cerr << "ERROR: Game strategy mismatch!" << std::endl;
        return 1;
    }

    // Verify Events
    if (loaded_game.events.size() != 2) {
        std::cerr << "ERROR: Game events size mismatch! Expected 2, got " << loaded_game.events.size() << std::endl;
        return 1;
    }
    if (loaded_game.events[0].description != "Jump ball won by DAL.") {
        std::cerr << "ERROR: Game event 1 description mismatch!" << std::endl;
        return 1;
    }
    if (loaded_game.events[1].description != "Luka Doncic hits a stepback three-pointer!") {
        std::cerr << "ERROR: Game event 2 description mismatch!" << std::endl;
        return 1;
    }

    // Clean up database file
    std::remove(db_path.c_str());

    std::cout << "[PERSISTENCE TEST] PASS!" << std::endl;
    return 0;
}
