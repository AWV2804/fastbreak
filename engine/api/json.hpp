#pragma once

#include "engine/api/dtos.hpp"
#include "engine/game/manager_command.hpp"

#include <string>

std::string toJson(const GameSnapshotDTO& snapshot);
std::string toJson(const CommandValidation& validation);
