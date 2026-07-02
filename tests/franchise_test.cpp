#include "engine/franchise/franchise_engine.hpp"

#include <iostream>
#include <vector>
#include <unordered_map>
#include <algorithm>

namespace {
Player makeDummyPlayer(const std::string& id, const std::string& name, float skill = 0.6f, int age = 25) {
    Player p;
    p.player_id = id;
    p.name = name;
    p.age = age;
    p.primary_position = Position::PG;
    p.ratings.current.inside_scoring = skill;
    p.ratings.current.mid_range = skill;
    p.ratings.current.three_point = skill;
    p.ratings.current.free_throw = skill;
    p.ratings.current.passing = skill;
    p.ratings.current.dribbling = skill;
    p.ratings.current.offensive_iq = skill;
    p.ratings.current.perimeter_defense = skill;
    p.ratings.current.interior_defense = skill;
    p.ratings.current.stealing = skill;
    p.ratings.current.blocking = skill;
    p.ratings.current.rebounding = skill;
    p.ratings.current.speed = skill;
    p.ratings.current.strength = skill;
    p.ratings.current.stamina = skill;
    p.ratings.potential = p.ratings.current;
    p.fatigue = 0.0f;
    return p;
}

void registerTeam(FranchiseEngine& eng, const std::string& id, const std::string& name, float skill = 0.6f) {
    std::vector<std::string> roster;
    CourtLineup lineup;

    // Create 5 players for this team
    std::string pg_id = id + "_pg";
    std::string sg_id = id + "_sg";
    std::string sf_id = id + "_sf";
    std::string pf_id = id + "_pf";
    std::string c_id  = id + "_c";

    eng.registerPlayer(makeDummyPlayer(pg_id, name + " PG", skill, 25));
    eng.registerPlayer(makeDummyPlayer(sg_id, name + " SG", skill, 25));
    eng.registerPlayer(makeDummyPlayer(sf_id, name + " SF", skill, 25));
    eng.registerPlayer(makeDummyPlayer(pf_id, name + " PF", skill, 25));
    eng.registerPlayer(makeDummyPlayer(c_id, name + " C", skill, 25));

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
    config.roster_limit = 15;
    FranchiseEngine eng(config, 12345U);

    std::cout << "[FRANCHISE TEST] Registering teams..." << std::endl;
    registerTeam(eng, "BOS", "Boston Celtics", 0.70f); // Strong team
    registerTeam(eng, "MIA", "Miami Heat", 0.60f);
    registerTeam(eng, "LAL", "Los Angeles Lakers", 0.58f);
    registerTeam(eng, "DAL", "Dallas Mavericks", 0.62f);

    // Register a star player on DAL and a bench warmer on BOS to test trades
    eng.registerPlayer(makeDummyPlayer("DAL_star", "Luka Doncic", 0.95f, 23));
    eng.registerPlayer(makeDummyPlayer("BOS_bench", "Weak Prospect", 0.35f, 29));
    
    // Add them to their respective team rosters manually
    {
        // Add to DAL
        FranchiseTeam dal = eng.getTeam("DAL");
        std::vector<std::string> roster = dal.roster_player_ids;
        roster.push_back("DAL_star");
        eng.addTeam("DAL", dal.team_name, dal.active_lineup, roster);
    }
    {
        // Add to BOS
        FranchiseTeam bos = eng.getTeam("BOS");
        std::vector<std::string> roster = bos.roster_player_ids;
        roster.push_back("BOS_bench");
        eng.addTeam("BOS", bos.team_name, bos.active_lineup, roster);
    }

    std::cout << "[FRANCHISE TEST] Testing Trading AI..." << std::endl;
    // Proposal 1: User tries to trade Weak Prospect for Luka Doncic (unfavorable for AI)
    TradeProposal prop1;
    prop1.team_a_id = "BOS";
    prop1.team_b_id = "DAL";
    prop1.assets_sent = {"BOS_bench"};
    prop1.assets_received = {"DAL_star"};

    TradeResult res1 = eng.evaluateTrade(prop1);
    std::cout << "  Unfavorable trade proposal accepted? " << (res1.accepted ? "Yes" : "No") << " | Reason: " << res1.reason << std::endl;
    if (res1.accepted) {
        std::cerr << "ERROR: AI accepted an extremely unfair trade!" << std::endl;
        return 1;
    }

    // Proposal 2: User overpays, trading Luka Doncic for Weak Prospect (favorable for AI)
    TradeProposal prop2;
    prop2.team_a_id = "DAL";
    prop2.team_b_id = "BOS";
    prop2.assets_sent = {"DAL_star"};
    prop2.assets_received = {"BOS_bench"};

    TradeResult res2 = eng.evaluateTrade(prop2);
    std::cout << "  Favorable trade proposal accepted? " << (res2.accepted ? "Yes" : "No") << " | Reason: " << res2.reason << std::endl;
    if (!res2.accepted) {
        std::cerr << "ERROR: AI rejected a highly favorable trade! Reason: " << res2.reason << std::endl;
        return 1;
    }

    // Execute trade prop2
    std::cout << "[FRANCHISE TEST] Executing trade..." << std::endl;
    if (!eng.executeTrade(prop2)) {
        std::cerr << "ERROR: Executing trade failed!" << std::endl;
        return 1;
    }

    // Verify rosters post trade
    FranchiseTeam dal_post = eng.getTeam("DAL");
    FranchiseTeam bos_post = eng.getTeam("BOS");
    if (std::find(dal_post.roster_player_ids.begin(), dal_post.roster_player_ids.end(), "BOS_bench") == dal_post.roster_player_ids.end() ||
        std::find(bos_post.roster_player_ids.begin(), bos_post.roster_player_ids.end(), "DAL_star") == bos_post.roster_player_ids.end()) {
        std::cerr << "ERROR: Rosters not updated correctly after trade!" << std::endl;
        return 1;
    }
    std::cout << "  Rosters successfully swapped assets." << std::endl;

    std::cout << "[FRANCHISE TEST] Testing Rookie Draft System..." << std::endl;
    // Generate draft class
    eng.generateDraftClass();
    const auto& draft_class = eng.getDraftClass();
    std::cout << "  Draft class size: " << draft_class.size() << " (Expected 8)" << std::endl;
    if (draft_class.size() != 8) {
        std::cerr << "ERROR: Rookie draft class size mismatch!" << std::endl;
        return 1;
    }

    // Generate schedule and standings for order check
    eng.generateSchedule();
    std::vector<std::string> logs;
    while (eng.currentDay() <= 6) {
        eng.simulateDay("DAL", logs);
    }

    std::vector<std::string> draft_order = eng.getDraftOrder();
    std::cout << "  Draft Order (reverse of standings):" << std::endl;
    for (size_t i = 0; i < draft_order.size(); ++i) {
        std::cout << "    " << (i + 1) << ". " << draft_order[i] << std::endl;
    }

    // Execute draft round
    std::string first_pick_team = draft_order[0];
    std::string chosen_rookie_id = draft_class[0].prospect_id;
    std::string chosen_rookie_name = draft_class[0].name;

    std::cout << "  Drafting rookie " << chosen_rookie_name << " (" << chosen_rookie_id << ") to team " << first_pick_team << std::endl;
    if (!eng.executePlayerDraftPick(first_pick_team, chosen_rookie_id)) {
        std::cerr << "ERROR: Player draft pick failed!" << std::endl;
        return 1;
    }

    // Verify roster size of drafted team increased
    FranchiseTeam picker_team = eng.getTeam(first_pick_team);
    if (std::find(picker_team.roster_player_ids.begin(), picker_team.roster_player_ids.end(), chosen_rookie_id) == picker_team.roster_player_ids.end()) {
        std::cerr << "ERROR: Drafted rookie not found on team roster!" << std::endl;
        return 1;
    }

    // AI Picks
    std::string second_pick_team = draft_order[1];
    std::string ai_drafted_name = eng.executeAIDraftPick(second_pick_team);
    std::cout << "  AI Team " << second_pick_team << " drafted: " << ai_drafted_name << std::endl;
    if (ai_drafted_name.empty()) {
        std::cerr << "ERROR: AI draft pick returned empty name!" << std::endl;
        return 1;
    }

    std::cout << "[FRANCHISE TEST] PASS!" << std::endl;
    return 0;
}
