#include "ui/backends/imgui/hud/scoreboard.hpp"

#include <imgui.h>

namespace ui {

void ImGuiHudScoreboard::setEntries(const std::vector<ScoreboardEntry> &entriesIn) {
    entries = entriesIn;
}

void ImGuiHudScoreboard::draw(ImGuiIO &io) {
    ImGui::SetNextWindowPos(ImVec2(20, 20));
    ImGui::SetNextWindowSize(ImVec2(500, 200));
    ImGui::SetNextWindowBgAlpha(0.0f);

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
}

} // namespace ui
