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
