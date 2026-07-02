#include "engine/persistence/sqlite_store.hpp"

#include <sqlite3.h>

#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <algorithm>

namespace {

std::string join(const std::vector<std::string>& items) {
    std::ostringstream oss;
    for (size_t i = 0; i < items.size(); ++i) {
        if (i > 0) {
            oss << ';';
        }
        oss << items[i];
    }
    return oss.str();
}

std::vector<std::string> split(const std::string& blob) {
    std::vector<std::string> out;
    std::string item;
    std::istringstream iss(blob);
    while (std::getline(iss, item, ';')) {
        if (!item.empty()) {
            out.push_back(item);
        }
    }
    return out;
}

std::string serializeLineup(const CourtLineup& lineup) {
    std::ostringstream ss;
    ss << lineup.pg << "," << lineup.sg << "," << lineup.sf << "," << lineup.pf << "," << lineup.c;
    return ss.str();
}

CourtLineup deserializeLineup(const std::string& str) {
    std::istringstream ss(str);
    std::string pg, sg, sf, pf, c;
    std::getline(ss, pg, ',');
    std::getline(ss, sg, ',');
    std::getline(ss, sf, ',');
    std::getline(ss, pf, ',');
    std::getline(ss, c, ',');
    CourtLineup lineup;
    lineup.pg = pg;
    lineup.sg = sg;
    lineup.sf = sf;
    lineup.pf = pf;
    lineup.c = c;
    return lineup;
}

std::string serializeStrategy(const TeamStrategy& strat) {
    std::ostringstream ss;
    ss << static_cast<int>(strat.pace) << "," << static_cast<int>(strat.offense) << "," << static_cast<int>(strat.defense);
    return ss.str();
}

TeamStrategy deserializeStrategy(const std::string& str) {
    std::istringstream ss(str);
    std::string pace_s, off_s, def_s;
    std::getline(ss, pace_s, ',');
    std::getline(ss, off_s, ',');
    std::getline(ss, def_s, ',');
    TeamStrategy strat;
    strat.pace = static_cast<StrategyPace>(std::stoi(pace_s));
    strat.offense = static_cast<StrategyOffense>(std::stoi(off_s));
    strat.defense = static_cast<StrategyDefense>(std::stoi(def_s));
    return strat;
}

std::string safeColumnText(sqlite3_stmt* stmt, int col) {
    const char* text = reinterpret_cast<const char*>(sqlite3_column_text(stmt, col));
    return text ? std::string(text) : "";
}

}  // namespace

struct SqliteStore::Impl {
    sqlite3* db = nullptr;
};

SqliteStore::SqliteStore(const std::string& db_path) : impl_(new Impl()) {
    if (sqlite3_open(db_path.c_str(), &impl_->db) != SQLITE_OK) {
        throw std::runtime_error("Failed to open sqlite database");
    }
}

SqliteStore::~SqliteStore() {
    if (impl_ != nullptr) {
        if (impl_->db != nullptr) {
            sqlite3_close(impl_->db);
        }
        delete impl_;
    }
}

bool SqliteStore::migrate() {
    const char* ddl = R"SQL(
        CREATE TABLE IF NOT EXISTS schema_migrations (
            version INTEGER PRIMARY KEY
        );

        CREATE TABLE IF NOT EXISTS players (
            save_id TEXT NOT NULL,
            player_id TEXT NOT NULL,
            name TEXT NOT NULL,
            age INTEGER NOT NULL,
            primary_position INTEGER NOT NULL,
            inside_scoring REAL NOT NULL,
            mid_range REAL NOT NULL,
            three_point REAL NOT NULL,
            free_throw REAL NOT NULL,
            passing REAL NOT NULL,
            dribbling REAL NOT NULL,
            offensive_iq REAL NOT NULL,
            perimeter_defense REAL NOT NULL,
            interior_defense REAL NOT NULL,
            stealing REAL NOT NULL,
            blocking REAL NOT NULL,
            rebounding REAL NOT NULL,
            speed REAL NOT NULL,
            strength REAL NOT NULL,
            stamina REAL NOT NULL,
            fatigue REAL NOT NULL,
            PRIMARY KEY (save_id, player_id)
        );

        CREATE TABLE IF NOT EXISTS rosters (
            save_id TEXT NOT NULL,
            team_id TEXT NOT NULL,
            player_ids TEXT NOT NULL,
            PRIMARY KEY (save_id, team_id)
        );

        CREATE TABLE IF NOT EXISTS games (
            save_id TEXT NOT NULL,
            game_id TEXT NOT NULL,
            quarter INTEGER NOT NULL,
            game_clock REAL NOT NULL,
            shot_clock REAL NOT NULL,
            home_score INTEGER NOT NULL,
            away_score INTEGER NOT NULL,
            home_fouls INTEGER NOT NULL,
            away_fouls INTEGER NOT NULL,
            home_timeouts INTEGER NOT NULL,
            away_timeouts INTEGER NOT NULL,
            possession_team_index INTEGER NOT NULL,
            ball_handler_id TEXT NOT NULL,
            game_over INTEGER NOT NULL,
            seed INTEGER NOT NULL,
            home_lineup TEXT NOT NULL,
            away_lineup TEXT NOT NULL,
            home_roster TEXT NOT NULL,
            away_roster TEXT NOT NULL,
            home_strategy TEXT NOT NULL,
            away_strategy TEXT NOT NULL,
            event_blob TEXT NOT NULL,
            PRIMARY KEY (save_id, game_id)
        );
    )SQL";

    char* err = nullptr;
    if (sqlite3_exec(impl_->db, ddl, nullptr, nullptr, &err) != SQLITE_OK) {
        sqlite3_free(err);
        return false;
    }

    sqlite3_exec(impl_->db, "INSERT OR IGNORE INTO schema_migrations(version) VALUES (1);", nullptr, nullptr, nullptr);
    return true;
}

bool SqliteStore::saveBundle(const SaveBundle& bundle, const std::string& mode) {
    if (mode == "overwrite") {
        sqlite3_stmt* clear_stmt = nullptr;
        sqlite3_prepare_v2(impl_->db, "DELETE FROM players WHERE save_id=?;", -1, &clear_stmt, nullptr);
        sqlite3_bind_text(clear_stmt, 1, bundle.save_id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(clear_stmt);
        sqlite3_finalize(clear_stmt);

        sqlite3_prepare_v2(impl_->db, "DELETE FROM rosters WHERE save_id=?;", -1, &clear_stmt, nullptr);
        sqlite3_bind_text(clear_stmt, 1, bundle.save_id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(clear_stmt);
        sqlite3_finalize(clear_stmt);

        sqlite3_prepare_v2(impl_->db, "DELETE FROM games WHERE save_id=?;", -1, &clear_stmt, nullptr);
        sqlite3_bind_text(clear_stmt, 1, bundle.save_id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(clear_stmt);
        sqlite3_finalize(clear_stmt);
    }

    sqlite3_exec(impl_->db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);

    const char* player_sql = R"SQL(
        INSERT OR REPLACE INTO players (
            save_id, player_id, name, age, primary_position,
            inside_scoring, mid_range, three_point, free_throw,
            passing, dribbling, offensive_iq,
            perimeter_defense, interior_defense, stealing, blocking,
            rebounding, speed, strength, stamina, fatigue
        ) VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?);
    )SQL";

    sqlite3_stmt* player_stmt = nullptr;
    sqlite3_prepare_v2(impl_->db, player_sql, -1, &player_stmt, nullptr);

    for (const Player& p : bundle.players) {
        sqlite3_bind_text(player_stmt, 1, bundle.save_id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(player_stmt, 2, p.player_id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(player_stmt, 3, p.name.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(player_stmt, 4, p.age);
        sqlite3_bind_int(player_stmt, 5, static_cast<int>(p.primary_position));
        
        sqlite3_bind_double(player_stmt, 6, p.ratings.current.inside_scoring);
        sqlite3_bind_double(player_stmt, 7, p.ratings.current.mid_range);
        sqlite3_bind_double(player_stmt, 8, p.ratings.current.three_point);
        sqlite3_bind_double(player_stmt, 9, p.ratings.current.free_throw);
        sqlite3_bind_double(player_stmt, 10, p.ratings.current.passing);
        sqlite3_bind_double(player_stmt, 11, p.ratings.current.dribbling);
        sqlite3_bind_double(player_stmt, 12, p.ratings.current.offensive_iq);
        sqlite3_bind_double(player_stmt, 13, p.ratings.current.perimeter_defense);
        sqlite3_bind_double(player_stmt, 14, p.ratings.current.interior_defense);
        sqlite3_bind_double(player_stmt, 15, p.ratings.current.stealing);
        sqlite3_bind_double(player_stmt, 16, p.ratings.current.blocking);
        sqlite3_bind_double(player_stmt, 17, p.ratings.current.rebounding);
        sqlite3_bind_double(player_stmt, 18, p.ratings.current.speed);
        sqlite3_bind_double(player_stmt, 19, p.ratings.current.strength);
        sqlite3_bind_double(player_stmt, 20, p.ratings.current.stamina);
        sqlite3_bind_double(player_stmt, 21, p.fatigue);

        sqlite3_step(player_stmt);
        sqlite3_reset(player_stmt);
        sqlite3_clear_bindings(player_stmt);
    }
    sqlite3_finalize(player_stmt);

    const char* roster_sql = R"SQL(
        INSERT OR REPLACE INTO rosters (
            save_id, team_id, player_ids
        ) VALUES (?,?,?);
    )SQL";

    sqlite3_stmt* roster_stmt = nullptr;
    sqlite3_prepare_v2(impl_->db, roster_sql, -1, &roster_stmt, nullptr);

    for (const TeamRoster& roster : bundle.rosters) {
        sqlite3_bind_text(roster_stmt, 1, bundle.save_id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(roster_stmt, 2, roster.team_id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(roster_stmt, 3, join(roster.roster_player_ids).c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(roster_stmt);
        sqlite3_reset(roster_stmt);
        sqlite3_clear_bindings(roster_stmt);
    }
    sqlite3_finalize(roster_stmt);

    const char* game_sql = R"SQL(
        INSERT OR REPLACE INTO games (
            save_id, game_id, quarter, game_clock, shot_clock,
            home_score, away_score, home_fouls, away_fouls,
            home_timeouts, away_timeouts, possession_team_index,
            ball_handler_id, game_over, seed,
            home_lineup, away_lineup, home_roster, away_roster,
            home_strategy, away_strategy, event_blob
        ) VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?);
    )SQL";

    sqlite3_stmt* game_stmt = nullptr;
    sqlite3_prepare_v2(impl_->db, game_sql, -1, &game_stmt, nullptr);

    for (const GameState& game : bundle.games) {
        std::ostringstream events;
        for (size_t i = 0; i < game.events.size(); ++i) {
            if (i > 0) {
                events << "||";
            }
            events << game.events[i].quarter << ',' << game.events[i].clock << ',' << game.events[i].description;
        }

        std::string home_l_str = serializeLineup(game.lineups[0]);
        std::string away_l_str = serializeLineup(game.lineups[1]);
        std::string home_r_str = join(game.rosters[0]);
        std::string away_r_str = join(game.rosters[1]);
        std::string home_s_str = serializeStrategy(game.strategies[0]);
        std::string away_s_str = serializeStrategy(game.strategies[1]);

        sqlite3_bind_text(game_stmt, 1, bundle.save_id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(game_stmt, 2, game.game_id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(game_stmt, 3, game.quarter);
        sqlite3_bind_double(game_stmt, 4, game.game_clock);
        sqlite3_bind_double(game_stmt, 5, game.shot_clock);
        sqlite3_bind_int(game_stmt, 6, game.score[0]);
        sqlite3_bind_int(game_stmt, 7, game.score[1]);
        sqlite3_bind_int(game_stmt, 8, game.team_fouls[0]);
        sqlite3_bind_int(game_stmt, 9, game.team_fouls[1]);
        sqlite3_bind_int(game_stmt, 10, game.timeouts_remaining[0]);
        sqlite3_bind_int(game_stmt, 11, game.timeouts_remaining[1]);
        sqlite3_bind_int(game_stmt, 12, game.possession_team_index);
        sqlite3_bind_text(game_stmt, 13, game.ball_handler_id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(game_stmt, 14, game.game_over ? 1 : 0);
        sqlite3_bind_int64(game_stmt, 15, static_cast<sqlite3_int64>(game.seed));
        
        sqlite3_bind_text(game_stmt, 16, home_l_str.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(game_stmt, 17, away_l_str.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(game_stmt, 18, home_r_str.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(game_stmt, 19, away_r_str.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(game_stmt, 20, home_s_str.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(game_stmt, 21, away_s_str.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(game_stmt, 22, events.str().c_str(), -1, SQLITE_TRANSIENT);

        sqlite3_step(game_stmt);
        sqlite3_reset(game_stmt);
        sqlite3_clear_bindings(game_stmt);
    }
    sqlite3_finalize(game_stmt);

    sqlite3_exec(impl_->db, "COMMIT;", nullptr, nullptr, nullptr);
    return true;
}

SaveBundle SqliteStore::loadBundle(const std::string& save_id) const {
    SaveBundle bundle;
    bundle.save_id = save_id;

    sqlite3_stmt* stmt = nullptr;

    const char* p_query = R"SQL(
        SELECT player_id, name, age, primary_position,
               inside_scoring, mid_range, three_point, free_throw,
               passing, dribbling, offensive_iq,
               perimeter_defense, interior_defense, stealing, blocking,
               rebounding, speed, strength, stamina, fatigue
        FROM players WHERE save_id=?;
    )SQL";

    sqlite3_prepare_v2(impl_->db, p_query, -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, save_id.c_str(), -1, SQLITE_TRANSIENT);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Player p;
        p.player_id = safeColumnText(stmt, 0);
        p.name = safeColumnText(stmt, 1);
        p.age = sqlite3_column_int(stmt, 2);
        p.primary_position = static_cast<Position>(sqlite3_column_int(stmt, 3));
        
        p.ratings.current.inside_scoring = static_cast<float>(sqlite3_column_double(stmt, 4));
        p.ratings.current.mid_range = static_cast<float>(sqlite3_column_double(stmt, 5));
        p.ratings.current.three_point = static_cast<float>(sqlite3_column_double(stmt, 6));
        p.ratings.current.free_throw = static_cast<float>(sqlite3_column_double(stmt, 7));
        p.ratings.current.passing = static_cast<float>(sqlite3_column_double(stmt, 8));
        p.ratings.current.dribbling = static_cast<float>(sqlite3_column_double(stmt, 9));
        p.ratings.current.offensive_iq = static_cast<float>(sqlite3_column_double(stmt, 10));
        p.ratings.current.perimeter_defense = static_cast<float>(sqlite3_column_double(stmt, 11));
        p.ratings.current.interior_defense = static_cast<float>(sqlite3_column_double(stmt, 12));
        p.ratings.current.stealing = static_cast<float>(sqlite3_column_double(stmt, 13));
        p.ratings.current.blocking = static_cast<float>(sqlite3_column_double(stmt, 14));
        p.ratings.current.rebounding = static_cast<float>(sqlite3_column_double(stmt, 15));
        p.ratings.current.speed = static_cast<float>(sqlite3_column_double(stmt, 16));
        p.ratings.current.strength = static_cast<float>(sqlite3_column_double(stmt, 17));
        p.ratings.current.stamina = static_cast<float>(sqlite3_column_double(stmt, 18));
        
        p.ratings.potential = p.ratings.current;
        p.fatigue = static_cast<float>(sqlite3_column_double(stmt, 19));

        bundle.players.push_back(p);
    }
    sqlite3_finalize(stmt);

    sqlite3_prepare_v2(impl_->db, "SELECT team_id, player_ids FROM rosters WHERE save_id=?;", -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, save_id.c_str(), -1, SQLITE_TRANSIENT);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        TeamRoster roster;
        roster.team_id = safeColumnText(stmt, 0);
        roster.roster_player_ids = split(safeColumnText(stmt, 1));
        bundle.rosters.push_back(roster);
    }
    sqlite3_finalize(stmt);

    const char* g_query = R"SQL(
        SELECT game_id, quarter, game_clock, shot_clock,
               home_score, away_score, home_fouls, away_fouls,
               home_timeouts, away_timeouts, possession_team_index,
               ball_handler_id, game_over, seed,
               home_lineup, away_lineup, home_roster, away_roster,
               home_strategy, away_strategy, event_blob
        FROM games WHERE save_id=?;
    )SQL";

    sqlite3_prepare_v2(impl_->db, g_query, -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, save_id.c_str(), -1, SQLITE_TRANSIENT);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        GameState game;
        game.game_id = safeColumnText(stmt, 0);
        game.quarter = sqlite3_column_int(stmt, 1);
        game.game_clock = static_cast<float>(sqlite3_column_double(stmt, 2));
        game.shot_clock = static_cast<float>(sqlite3_column_double(stmt, 3));
        game.score[0] = sqlite3_column_int(stmt, 4);
        game.score[1] = sqlite3_column_int(stmt, 5);
        game.team_fouls[0] = sqlite3_column_int(stmt, 6);
        game.team_fouls[1] = sqlite3_column_int(stmt, 7);
        game.timeouts_remaining[0] = sqlite3_column_int(stmt, 8);
        game.timeouts_remaining[1] = sqlite3_column_int(stmt, 9);
        game.possession_team_index = sqlite3_column_int(stmt, 10);
        game.ball_handler_id = safeColumnText(stmt, 11);
        game.game_over = sqlite3_column_int(stmt, 12) != 0;
        game.seed = static_cast<std::uint_fast32_t>(sqlite3_column_int64(stmt, 13));

        game.lineups[0] = deserializeLineup(safeColumnText(stmt, 14));
        game.lineups[1] = deserializeLineup(safeColumnText(stmt, 15));
        game.rosters[0] = split(safeColumnText(stmt, 16));
        game.rosters[1] = split(safeColumnText(stmt, 17));
        game.strategies[0] = deserializeStrategy(safeColumnText(stmt, 18));
        game.strategies[1] = deserializeStrategy(safeColumnText(stmt, 19));

        std::string event_blob = safeColumnText(stmt, 20);
        std::istringstream rows(event_blob);
        std::string row;
        while (std::getline(rows, row, '|')) {
            if (row.empty() || row == "|") {
                continue;
            }
            if (row.back() == '|') {
                row.pop_back();
            }
            std::istringstream fields(row);
            std::string cell;
            GameEventLog ev;
            if (std::getline(fields, cell, ',')) {
                ev.quarter = std::stoi(cell);
            }
            if (std::getline(fields, cell, ',')) {
                ev.clock = std::stof(cell);
            }
            if (std::getline(fields, cell)) {
                ev.description = cell;
            }
            game.events.push_back(ev);
        }

        bundle.games.push_back(game);
    }
    sqlite3_finalize(stmt);

    return bundle;
}
