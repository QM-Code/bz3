#include "ui/backends/imgui/hud/fps.hpp"

#include <imgui.h>

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
    ImGui::Text("FPS: %.1f", io.Framerate);
    ImGui::End();
}

} // namespace ui
