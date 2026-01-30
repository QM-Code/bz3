#pragma once

#include <string>
#include <vector>

#include "ui/types.hpp"
#include "ui/frontends/imgui/hud/chat.hpp"
#include "ui/frontends/imgui/hud/crosshair.hpp"
#include "ui/frontends/imgui/hud/dialog.hpp"
#include "ui/frontends/imgui/hud/fps.hpp"
#include "ui/frontends/imgui/hud/radar.hpp"
#include "ui/frontends/imgui/hud/scoreboard.hpp"

struct ImGuiIO;
struct ImFont;

namespace ui {

class ImGuiHud {
public:
    void setScoreboardEntries(const std::vector<ScoreboardEntry> &entries);
    void setDialogText(const std::string &text);
    void setDialogVisible(bool show);
    void setRadarTexture(const graphics::TextureHandle& texture);
    void setScoreboardVisible(bool show);
    void setChatVisible(bool show);
    void setRadarVisible(bool show);
    void setCrosshairVisible(bool show);
    void setFpsValue(float value);
    void setChatLines(const std::vector<std::string> &lines);

    void addConsoleLine(const std::string &playerName, const std::string &line);
    std::string getChatInputBuffer() const;
    void clearChatInputBuffer();
    void focusChatInput();
    bool getChatInputFocus() const;

    void setShowFps(bool show);
    void draw(ImGuiIO &io, ImFont *bigFont);

private:
    ImGuiHudScoreboard scoreboard;
    ImGuiHudDialog dialog;
    ImGuiHudRadar radar;
    ImGuiHudChat chat;
    ImGuiHudCrosshair crosshair;
    ImGuiHudFps fps;
    bool scoreboardVisible = true;
    bool chatVisible = true;
    bool radarVisible = true;
    bool crosshairVisible = true;
};

} // namespace ui
