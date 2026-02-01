#include "ui/frontends/imgui/hud/fps.hpp"

#include <imgui.h>
#include <algorithm>
#include <cstdio>
#include "karma/common/i18n.hpp"

namespace ui {

void ImGuiHudFps::setVisible(bool show) {
    visible = show;
}

void ImGuiHudFps::setValue(float value) {
    fpsValue = value;
}

void ImGuiHudFps::draw(ImGuiIO &io, const ImVec4 &backgroundColor) {
    if (!visible) {
        return;
    }
    const float margin = 16.0f;
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - margin, margin), ImGuiCond_Always, ImVec2(1.0f, 0.0f));
    ImVec4 bg = backgroundColor;
    bg.x = std::clamp(bg.x, 0.0f, 1.0f);
    bg.y = std::clamp(bg.y, 0.0f, 1.0f);
    bg.z = std::clamp(bg.z, 0.0f, 1.0f);
    bg.w = std::clamp(bg.w, 0.0f, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, bg);
    ImGui::Begin("##FPSOverlay", nullptr,
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar |
    ImGuiWindowFlags_NoSavedSettings |
    ImGuiWindowFlags_AlwaysAutoResize);
    char fpsBuffer[32];
    std::snprintf(fpsBuffer, sizeof(fpsBuffer), "%.1f", fpsValue);
    const std::string fpsText = karma::i18n::Get().format("ui.hud.fps_label", {{"value", fpsBuffer}});
    ImGui::TextUnformatted(fpsText.c_str());
    ImGui::End();
    ImGui::PopStyleColor();
}

} // namespace ui
