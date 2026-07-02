#include "engine/franchise/franchise_engine.hpp"

#include <iostream>
#include <vector>
#include <unordered_map>

namespace {
Player makeDummyPlayer(const std::string& id, const std::string& name) {
    Player p;
    p.player_id = id;
    p.name = name;
    p.age = 25;
    p.primary_position = Position::PG;
    p.ratings.current.inside_scoring = 0.6f;
    p.ratings.current.mid_range = 0.6f;
    p.ratings.current.three_point = 0.6f;
    p.ratings.current.passing = 0.6f;
    p.ratings.current.perimeter_defense = 0.6f;
    p.ratings.current.interior_defense = 0.6f;
    p.ratings.current.rebounding = 0.6f;
    p.ratings.current.stamina = 0.6f;
    p.ratings.potential = p.ratings.current;
    p.fatigue = 0.0f;
    return p;
}

void registerTeam(FranchiseEngine& eng, const std::string& id, const std::string& name) {
    std::vector<std::string> roster;
    CourtLineup lineup;

    // Create 5 players for this team
    std::string pg_id = id + "_pg";
    std::string sg_id = id + "_sg";
    std::string sf_id = id + "_sf";
    std::string pf_id = id + "_pf";
    std::string c_id  = id + "_c";

    eng.registerPlayer(makeDummyPlayer(pg_id, name + " PG"));
    eng.registerPlayer(makeDummyPlayer(sg_id, name + " SG"));
    eng.registerPlayer(makeDummyPlayer(sf_id, name + " SF"));
    eng.registerPlayer(makeDummyPlayer(pf_id, name + " PF"));
    eng.registerPlayer(makeDummyPlayer(c_id, name + " C"));

    roster = {pg_id, sg_id, sf_id, pf_id, c_id};
    lineup.pg = pg_id;
    lineup.sg = sg_id;
    lineup.sf = sf_id;
    lineup.pf = pf_id;
    lineup.c = c_id;

    eng.addTeam(id, name, lineup, roster);
}
}

int main() {
    std::cout << "[FRANCHISE TEST] Initializing FranchiseEngine..." << std::endl;
    LeagueConfig config;
    FranchiseEngine eng(config, 12345U);

    std::cout << "[FRANCHISE TEST] Registering teams..." << std::endl;
    registerTeam(eng, "BOS", "Boston Celtics");
    registerTeam(eng, "MIA", "Miami Heat");
    registerTeam(eng, "LAL", "Los Angeles Lakers");
    registerTeam(eng, "DAL", "Dallas Mavericks");

    std::cout << "[FRANCHISE TEST] Generating schedule..." << std::endl;
    eng.generateSchedule();

    const auto& schedule = eng.schedule();
    std::cout << "[FRANCHISE TEST] Generated " << schedule.size() << " games in schedule." << std::endl;
    if (schedule.size() != 12) { // 4 teams round-robin twice: 4 * 3 = 12 games
        std::cerr << "ERROR: Schedule size mismatch! Expected 12, got " << schedule.size() << std::endl;
        return 1;
    }

    std::cout << "[FRANCHISE TEST] Simulating season day-by-day..." << std::endl;
    std::vector<std::string> logs;
    // 4 teams, 6 rounds total (Berger table length = 3 rounds twice = 6 days)
    while (eng.currentDay() <= 6) {
        eng.simulateDay("DAL", logs);
        std::cout << "Day " << (eng.currentDay() - 1) << " logs:" << std::endl;
        for (const auto& log : logs) {
            std::cout << "  " << log << std::endl;
        }
    }

    std::cout << "[FRANCHISE TEST] Verifying final standings..." << std::endl;
    std::vector<TeamStanding> standings = eng.getStandings();
    if (standings.size() != 4) {
        std::cerr << "ERROR: Standings size mismatch! Expected 4, got " << standings.size() << std::endl;
        return 1;
    }

    int total_wins = 0;
    int total_losses = 0;
    for (const auto& stand : standings) {
        std::cout << "Team: " << stand.team_id << " | Wins: " << stand.wins 
                  << " | Losses: " << stand.losses << " | PTS+: " << stand.points_for 
                  << " | PTS-: " << stand.points_against << std::endl;
        total_wins += stand.wins;
        total_losses += stand.losses;

        if (stand.wins + stand.losses != 6) {
            std::cerr << "ERROR: Team " << stand.team_id << " played " 
                      << (stand.wins + stand.losses) << " games, expected 6!" << std::endl;
            return 1;
        }
    }

    if (total_wins != 12 || total_losses != 12) {
        std::cerr << "ERROR: Total wins/losses mismatch! Wins: " << total_wins 
                  << ", Losses: " << total_losses << std::endl;
        return 1;
    }

    // Verify ordering
    if (standings[0].wins < standings[1].wins) {
        std::cerr << "ERROR: Standings are not sorted correctly by wins!" << std::endl;
        return 1;
    }

    std::cout << "[FRANCHISE TEST] PASS!" << std::endl;
    return 0;
}
