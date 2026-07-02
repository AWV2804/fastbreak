#pragma once

#include "engine/game/game_state.hpp"
#include "engine/model/player.hpp"

#include <string>
#include <vector>

struct SaveBundle {
    std::string save_id;
    std::vector<Player> players;
    std::vector<TeamRoster> rosters;
    std::vector<GameState> games;
    LeagueConfig league_config;
};

class SqliteStore {
public:
    explicit SqliteStore(const std::string& db_path);
    ~SqliteStore();

    SqliteStore(const SqliteStore&) = delete;
    SqliteStore& operator=(const SqliteStore&) = delete;

    bool migrate();

    bool saveBundle(const SaveBundle& bundle, const std::string& mode);
    SaveBundle loadBundle(const std::string& save_id) const;

private:
    struct Impl;
    Impl* impl_;
};
