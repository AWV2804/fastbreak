#pragma once

#include <optional>
#include <string>

enum class ManagerCommandType {
    NONE,
    TIMEOUT,
    SUBSTITUTION,
    SET_PACE,
    SET_OFFENSE,
    SET_DEFENSE
};

struct ManagerCommand {
    ManagerCommandType type = ManagerCommandType::NONE;
    std::optional<std::string> payload;
};

struct CommandValidation {
    bool accepted = true;
    std::string reason = "";
};
