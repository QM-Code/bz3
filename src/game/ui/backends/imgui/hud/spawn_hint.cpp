#include "ui/backends/imgui/hud/spawn_hint.hpp"

#include <imgui.h>

namespace ui {

void ImGuiHudSpawnHint::setHint(const std::string &hint) {
    hintText = hint;
}

void ImGuiHudSpawnHint::setVisible(bool show) {
    visible = show;
}

void ImGuiHudSpawnHint::draw(ImGuiIO &io, ImFont *bigFont) {
    if (!visible) {
        return;
    }
    ImVec2 screenCenter = ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f);

    const char* text = hintText.c_str();
    const float fontScale = 0.55f;
    ImFont* font = bigFont ? bigFont : ImGui::GetFont();
    const float drawSize = font ? font->FontSize * fontScale : ImGui::GetFontSize() * fontScale;
    ImVec2 textSize;
    if (font) {
        textSize = font->CalcTextSizeA(drawSize, FLT_MAX, 0.0f, text);
    } else {
        const ImVec2 raw = ImGui::CalcTextSize(text);
        textSize = ImVec2(raw.x * fontScale, raw.y * fontScale);
    }

    const ImVec2 textPos(
        screenCenter.x - textSize.x * 0.5f,
        screenCenter.y - textSize.y * 0.5f
    );

    auto* drawList = ImGui::GetForegroundDrawList();
    const ImU32 shadowColor = IM_COL32(0, 0, 0, 180);
    const ImU32 textColor = IM_COL32(255, 255, 255, 255);
    const ImVec2 shadowOffset(2.0f, 2.0f);

    const ImVec2 shadowPos(textPos.x + shadowOffset.x, textPos.y + shadowOffset.y);
    drawList->AddText(font, drawSize, shadowPos, shadowColor, text);
    drawList->AddText(font, drawSize, textPos, textColor, text);
}

} // namespace ui
