#include "ui/frontends/imgui/hud/scoreboard.hpp"

#include <imgui.h>

#include <algorithm>

namespace ui {

void ImGuiHudScoreboard::setEntries(const std::vector<ScoreboardEntry> &entriesIn) {
    entries = entriesIn;
}

void ImGuiHudScoreboard::draw(ImGuiIO &io, const ImVec4 &backgroundColor) {
    ImVec4 bg = backgroundColor;
    bg.x = std::clamp(bg.x, 0.0f, 1.0f);
    bg.y = std::clamp(bg.y, 0.0f, 1.0f);
    bg.z = std::clamp(bg.z, 0.0f, 1.0f);
    bg.w = std::clamp(bg.w, 0.0f, 1.0f);
    ImGui::SetNextWindowPos(ImVec2(20, 20));
    ImGui::SetNextWindowSize(ImVec2(500, 200));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, bg);

    ImGui::Begin("TopLeftText", nullptr,
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoSavedSettings
    );

    for (const auto& entry : entries) {
        const char *prefix = "  ";
        if (entry.communityAdmin) {
            prefix = "@ ";
        } else if (entry.localAdmin) {
            prefix = "* ";
        } else if (entry.registeredUser) {
            prefix = "+ ";
        }
        ImGui::Text("%s%s  (%d)", prefix, entry.name.c_str(), entry.score);
    }
    ImGui::End();
    ImGui::PopStyleColor();
}

} // namespace ui
