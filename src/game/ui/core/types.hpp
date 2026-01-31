#pragma once

#include <string>

#include "karma/ui/types.hpp"

struct ScoreboardEntry {
    std::string name;
    int score;
    bool registeredUser = false;
    bool communityAdmin = false;
    bool localAdmin = false;
};
