#include "ui/backends/imgui/hud/hud.hpp"

#include <algorithm>
#include <imgui.h>

namespace ui {

void ImGuiHud::setScoreboardEntries(const std::vector<ScoreboardEntry> &entries) {
    scoreboard.setEntries(entries);
}

void ImGuiHud::setSpawnHint(const std::string &hint) {
    spawnHint.setHint(hint);
}

void ImGuiHud::displayDeathScreen(bool show) {
    spawnHint.setVisible(show);
}

void ImGuiHud::setRadarTextureId(unsigned int textureId) {
    radar.setTextureId(textureId);
}

void ImGuiHud::addConsoleLine(const std::string &playerName, const std::string &line) {
    chat.addLine(playerName, line);
}

std::string ImGuiHud::getChatInputBuffer() const {
    return chat.getSubmittedInput();
}

void ImGuiHud::clearChatInputBuffer() {
    chat.clearSubmittedInput();
}

void ImGuiHud::focusChatInput() {
    chat.focusInput();
}

bool ImGuiHud::getChatInputFocus() const {
    return chat.isFocused();
}

void ImGuiHud::setShowFps(bool show) {
    fps.setVisible(show);
}

void ImGuiHud::draw(ImGuiIO &io, ImFont *bigFont) {
    scoreboard.draw(io);

    const float margin = 12.0f;
    const float panelHeight = 260.0f;
    const float inputHeight = 34.0f;

    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImVec2 vpPos  = vp->Pos;
    ImVec2 vpSize = vp->Size;

    float radarSize = std::clamp(vpSize.y * 0.35f, 240.0f, 460.0f);
    radarSize = std::min(radarSize, vpSize.y - 2.0f * margin);
    radarSize = std::min(radarSize, vpSize.x - 2.0f * margin);

    const ImVec2 radarPos = ImVec2(vpPos.x + margin, vpPos.y + vpSize.y - margin - radarSize);
    const ImVec2 radarWindowSize = ImVec2(radarSize, radarSize);
    radar.draw(radarPos, radarWindowSize);

    const float consoleLeft = vpPos.x + margin + radarSize + margin;
    const float consoleWidth = std::max(50.0f, vpSize.x - (radarSize + 3.0f * margin));
    ImVec2 pos  = ImVec2(consoleLeft, vpPos.y + vpSize.y - margin - panelHeight);
    ImVec2 size = ImVec2(consoleWidth, panelHeight);

    chat.draw(pos, size, inputHeight);

    spawnHint.draw(io, bigFont);
    crosshair.draw(io);
    fps.draw(io);
}

} // namespace ui
