#pragma once

#include <string>

#include "ui/types.hpp"
#include "ui/backends/imgui/hud/chat.hpp"
#include "ui/backends/imgui/hud/crosshair.hpp"
#include "ui/backends/imgui/hud/fps.hpp"
#include "ui/backends/imgui/hud/radar.hpp"
#include "ui/backends/imgui/hud/scoreboard.hpp"
#include "ui/backends/imgui/hud/spawn_hint.hpp"

struct ImGuiIO;
struct ImFont;

namespace ui {

class ImGuiHud {
public:
    void setScoreboardEntries(const std::vector<ScoreboardEntry> &entries);
    void setSpawnHint(const std::string &hint);
    void displayDeathScreen(bool show);
    void setRadarTextureId(unsigned int textureId);

    void addConsoleLine(const std::string &playerName, const std::string &line);
    std::string getChatInputBuffer() const;
    void clearChatInputBuffer();
    void focusChatInput();
    bool getChatInputFocus() const;

    void setShowFps(bool show);
    void draw(ImGuiIO &io, ImFont *bigFont);

private:
    ImGuiHudScoreboard scoreboard;
    ImGuiHudSpawnHint spawnHint;
    ImGuiHudRadar radar;
    ImGuiHudChat chat;
    ImGuiHudCrosshair crosshair;
    ImGuiHudFps fps;
};

} // namespace ui
