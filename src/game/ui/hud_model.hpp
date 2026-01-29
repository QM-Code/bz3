#pragma once

#include <string>
#include <vector>

#include "ui/types.hpp"

namespace ui {

struct HudDialog {
    std::string text;
    bool visible = false;
};

struct HudVisibility {
    bool scoreboard = true;
    bool chat = true;
    bool radar = true;
    bool fps = false;
    bool crosshair = true;
};

struct HudModel {
    std::vector<ScoreboardEntry> scoreboardEntries;
    HudDialog dialog;
    HudVisibility visibility;
};

} // namespace ui
