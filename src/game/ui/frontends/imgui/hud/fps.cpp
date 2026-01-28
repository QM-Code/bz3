#include "ui/frontends/imgui/hud/fps.hpp"

#include <imgui.h>
#include <cstdio>
#include "common/i18n.hpp"

namespace ui {

void ImGuiHudFps::setVisible(bool show) {
    visible = show;
}

void ImGuiHudFps::draw(ImGuiIO &io) {
    if (!visible) {
        return;
    }
    const float margin = 16.0f;
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - margin, margin), ImGuiCond_Always, ImVec2(1.0f, 0.0f));
    ImGui::SetNextWindowBgAlpha(0.35f);
    ImGui::Begin("##FPSOverlay", nullptr,
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_AlwaysAutoResize);
    char fpsBuffer[32];
    std::snprintf(fpsBuffer, sizeof(fpsBuffer), "%.1f", io.Framerate);
    const std::string fpsText = bz::i18n::Get().format("ui.hud.fps_label", {{"value", fpsBuffer}});
    ImGui::TextUnformatted(fpsText.c_str());
    ImGui::End();
}

} // namespace ui
