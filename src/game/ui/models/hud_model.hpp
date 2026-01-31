#pragma once

#include <string>
#include <vector>

#include "ui/core/types.hpp"

namespace ui {

struct HudDialog {
    std::string text;
    bool visible = false;
};

struct HudVisibility {
    bool hud = true;
    bool scoreboard = true;
    bool chat = true;
    bool radar = true;
    bool fps = false;
    bool crosshair = true;
    bool quickMenu = false;
};

struct HudModel {
    std::vector<ScoreboardEntry> scoreboardEntries;
    std::vector<std::string> chatLines;
    HudDialog dialog;
    HudVisibility visibility;
    float fpsValue = 0.0f;
};

} // namespace ui
