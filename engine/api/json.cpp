#include "engine/api/json.hpp"

#include <sstream>

namespace {

std::string escapeJson(const std::string& s) {
    std::ostringstream out;
    for (char c : s) {
        switch (c) {
        case '\\':
            out << "\\\\";
            break;
        case '"':
            out << "\\\"";
            break;
        case '\n':
            out << "\\n";
            break;
        case '\r':
            out << "\\r";
            break;
        case '\t':
            out << "\\t";
            break;
        default:
            out << c;
            break;
        }
    }
    return out.str();
}

const char* jsonBool(bool v) {
    return v ? "true" : "false";
}

}  // namespace

std::string toJson(const GameSnapshotDTO& s) {
    std::ostringstream out;
    out << "{";
    out << "\"game_id\":\"" << escapeJson(s.game_id) << "\",";
    out << "\"quarter\":" << s.quarter << ",";
    out << "\"game_clock\":" << s.game_clock << ",";
    out << "\"shot_clock\":" << s.shot_clock << ",";
    out << "\"home_score\":" << s.home_score << ",";
    out << "\"away_score\":" << s.away_score << ",";
    out << "\"home_fouls\":" << s.home_fouls << ",";
    out << "\"away_fouls\":" << s.away_fouls << ",";
    out << "\"home_timeouts\":" << s.home_timeouts << ",";
    out << "\"away_timeouts\":" << s.away_timeouts << ",";
    out << "\"possession\":\"" << escapeJson(s.possession) << "\",";
    out << "\"ball_handler\":\"" << escapeJson(s.ball_handler) << "\",";
    out << "\"game_over\":" << jsonBool(s.game_over) << ",";
    
    // Serialize events array
    out << "\"latest_events\":[";
    for (size_t i = 0; i < s.latest_events.size(); ++i) {
        out << "\"" << escapeJson(s.latest_events[i]) << "\"";
        if (i + 1 < s.latest_events.size()) {
            out << ",";
        }
    }
    out << "],";

    out << "\"decision_point\":{";
    out << "\"awaiting_command\":" << jsonBool(s.decision_point.awaiting_command) << ",";
    out << "\"offense_team_index\":" << s.decision_point.offense_team_index << ",";
    out << "\"defense_team_index\":" << s.decision_point.defense_team_index;
    out << "}";
    out << "}";
    return out.str();
}

std::string toJson(const CommandValidation& validation) {
    std::ostringstream out;
    out << "{";
    out << "\"accepted\":" << jsonBool(validation.accepted) << ",";
    out << "\"reason\":\"" << escapeJson(validation.reason) << "\"";
    out << "}";
    return out.str();
}

std::string toJson(const ScheduledGame& game) {
    std::ostringstream out;
    out << "{";
    out << "\"game_id\":\"" << escapeJson(game.game_id) << "\",";
    out << "\"home_team_id\":\"" << escapeJson(game.home_team_id) << "\",";
    out << "\"away_team_id\":\"" << escapeJson(game.away_team_id) << "\",";
    out << "\"day\":" << game.day << ",";
    out << "\"simulated\":" << jsonBool(game.simulated) << ",";
    out << "\"home_score\":" << game.home_score << ",";
    out << "\"away_score\":" << game.away_score;
    out << "}";
    return out.str();
}

std::string toJson(const TeamStanding& standing) {
    std::ostringstream out;
    out << "{";
    out << "\"team_id\":\"" << escapeJson(standing.team_id) << "\",";
    out << "\"wins\":" << standing.wins << ",";
    out << "\"losses\":" << standing.losses << ",";
    out << "\"points_for\":" << standing.points_for << ",";
    out << "\"points_against\":" << standing.points_against;
    out << "}";
    return out.str();
}

std::string toJson(const RookieProspect& prospect) {
    std::ostringstream out;
    out << "{";
    out << "\"prospect_id\":\"" << escapeJson(prospect.prospect_id) << "\",";
    out << "\"name\":\"" << escapeJson(prospect.name) << "\",";
    out << "\"age\":" << prospect.age << ",";
    out << "\"position\":" << static_cast<int>(prospect.position) << ",";
    out << "\"rating\":" << prospect.ratings.current.mid_range << ",";
    out << "\"potential\":" << prospect.ratings.potential.mid_range;
    out << "}";
    return out.str();
}

std::string toJson(const TradeResult& result) {
    std::ostringstream out;
    out << "{";
    out << "\"accepted\":" << jsonBool(result.accepted) << ",";
    out << "\"reason\":\"" << escapeJson(result.reason) << "\",";
    out << "\"value_sent\":" << result.value_sent << ",";
    out << "\"value_received\":" << result.value_received;
    out << "}";
    return out.str();
}
