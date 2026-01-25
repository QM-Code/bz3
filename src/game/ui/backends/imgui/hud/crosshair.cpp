#include "ui/backends/imgui/hud/crosshair.hpp"

#include <imgui.h>

namespace ui {

void ImGuiHudCrosshair::draw(ImGuiIO &io) {
    const float boxSize = 50.0f;
    ImVec2 center(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f);

    ImVec2 p0(
        center.x - boxSize * 0.5f,
        center.y - boxSize * 0.5f
    );

    ImVec2 p1(
        center.x + boxSize * 0.5f,
        center.y + boxSize * 0.5f
    );

    ImGui::GetForegroundDrawList()->AddRect(
        p0,
        p1,
        IM_COL32(200, 200, 200, 180),
        0.0f,
        0,
        1.0f
    );
}

} // namespace ui
