#pragma once

#include "engine/api/dtos.hpp"
#include "engine/game/manager_command.hpp"

#include <string>

#include "engine/franchise/franchise_engine.hpp"

std::string toJson(const GameSnapshotDTO& snapshot);
std::string toJson(const CommandValidation& validation);
std::string toJson(const ScheduledGame& game);
std::string toJson(const TeamStanding& standing);
std::string toJson(const RookieProspect& prospect);
std::string toJson(const TradeResult& result);
