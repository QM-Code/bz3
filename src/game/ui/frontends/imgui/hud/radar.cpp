#include "ui/frontends/imgui/hud/radar.hpp"

#include <imgui.h>
#include <cstdint>

#include "engine/ui/imgui/texture_utils.hpp"

namespace ui {

void ImGuiHudRadar::setTexture(const graphics::TextureHandle& texture) {
    radarTexture = texture;
}

void ImGuiHudRadar::draw(const ImVec2 &pos, const ImVec2 &size) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));

    ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(size, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.35f);
    ImGui::Begin("##RadarPanel", nullptr,
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoSavedSettings
    );

    if (radarTexture.valid()) {
        ImGui::SetCursorPos(ImVec2(0.0f, 0.0f));
        const ImVec2 imageSize = ImGui::GetContentRegionAvail();
        ImTextureID tex = ui::ToImGuiTextureId(radarTexture);
        ImGui::Image(tex, imageSize, ImVec2(0, 1), ImVec2(1, 0));
    } else {
        ImGui::TextUnformatted("Radar");
        ImGui::Separator();
        ImGui::TextDisabled("(waiting for radar texture)");
    }
    ImGui::End();

    ImGui::PopStyleVar(4);
}

} // namespace ui
