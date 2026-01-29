#include "ui/frontends/imgui/console/console.hpp"

#include <string>
#include <vector>

#include "common/config_store.hpp"
#include "common/i18n.hpp"
#include "spdlog/spdlog.h"

namespace {

const std::vector<std::string> kLanguageCodes = {
    "en", "es", "fr", "de", "pt", "ru", "jp", "zh", "ko", "it", "hi", "ar"
};

bool DrawOnOffToggle(const char *label, bool &value) {
    const float labelWidth = 140.0f;
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);
    ImGui::SameLine(labelWidth);
    ImGui::PushID(label);
    bool changed = false;
    auto drawButton = [&](const char *text, bool active, bool nextValue) {
        if (active) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        }
        if (ImGui::SmallButton(text)) {
            value = nextValue;
            changed = true;
        }
        if (active) {
            ImGui::PopStyleColor(2);
        }
    };
    drawButton("On", value, true);
    ImGui::SameLine();
    drawButton("Off", !value, false);
    ImGui::PopID();
    return changed;
}

} // namespace

namespace ui {

void ConsoleView::drawSettingsPanel(const MessageColors &colors) {
    const uint64_t revision = bz::config::ConfigStore::Revision();
    if (!settingsLoaded || settingsRevision != revision) {
        settingsLoaded = true;
        settingsRevision = revision;
        settingsStatusText.clear();
        settingsStatusIsError = false;

        if (!bz::config::ConfigStore::Initialized()) {
            settingsStatusText = "Failed to load config; showing defaults.";
            settingsStatusIsError = true;
        }
        renderSettings.reset();
        hudSettings.reset();
        if (const auto *brightness = bz::config::ConfigStore::Get("render.brightness")) {
            if (brightness->is_number()) {
                renderSettings.setBrightness(static_cast<float>(brightness->get<double>()), false);
            }
        }
        auto applyHud = [&](const char *path, auto setter) {
            if (const auto *value = bz::config::ConfigStore::Get(path)) {
                if (value->is_boolean()) {
                    setter(value->get<bool>());
                }
            }
        };
        applyHud("ui.hud.scoreboard", [&](bool value) { hudSettings.setScoreboardVisible(value, false); });
        applyHud("ui.hud.chat", [&](bool value) { hudSettings.setChatVisible(value, false); });
        applyHud("ui.hud.radar", [&](bool value) { hudSettings.setRadarVisible(value, false); });
        applyHud("ui.hud.fps", [&](bool value) { hudSettings.setFpsVisible(value, false); });
        applyHud("ui.hud.crosshair", [&](bool value) { hudSettings.setCrosshairVisible(value, false); });
        std::string configuredLanguage = bz::i18n::Get().language();
        if (const auto *language = bz::config::ConfigStore::Get("language")) {
            if (language->is_string()) {
                configuredLanguage = language->get<std::string>();
            }
        }
        for (std::size_t i = 0; i < kLanguageCodes.size(); ++i) {
            if (kLanguageCodes[i] == configuredLanguage) {
                selectedLanguageIndex = static_cast<int>(i);
                break;
            }
        }
    }

    auto &i18n = bz::i18n::Get();
    ImGui::TextUnformatted(i18n.get("ui.settings.language_label").c_str());
    ImGui::SameLine();
    std::string selectedLangCode = (selectedLanguageIndex >= 0 &&
                                    selectedLanguageIndex < static_cast<int>(kLanguageCodes.size()))
        ? kLanguageCodes[static_cast<std::size_t>(selectedLanguageIndex)]
        : i18n.language();
    std::string selectedLangLabel = i18n.get("languages." + selectedLangCode);
    if (selectedLangLabel.empty()) {
        selectedLangLabel = selectedLangCode;
    }
    if (ImGui::BeginCombo("##LanguageSelect", selectedLangLabel.c_str())) {
        for (std::size_t i = 0; i < kLanguageCodes.size(); ++i) {
            const std::string &code = kLanguageCodes[i];
            std::string label = i18n.get("languages." + code);
            if (label.empty()) {
                label = code;
            }
            const bool isSelected = (selectedLanguageIndex == static_cast<int>(i));
            if (ImGui::Selectable(label.c_str(), isSelected)) {
                selectedLanguageIndex = static_cast<int>(i);
                if (!bz::config::ConfigStore::Set("language", code)) {
                    settingsStatusText = "Failed to save language.";
                    settingsStatusIsError = true;
                } else if (languageCallback) {
                    languageCallback(code);
                }
            }
            if (isSelected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    ImGui::TextUnformatted("Render");
    ImGui::Spacing();
    renderBrightnessDragging = false;
    float brightness = renderSettings.brightness();
    if (ImGui::SliderFloat("Brightness", &brightness, 0.2f, 3.0f, "%.2fx")) {
        applyRenderBrightness(brightness, true);
    }
    renderBrightnessDragging = ImGui::IsItemActive();
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        commitRenderBrightness();
    }
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::TextUnformatted("HUD");
    ImGui::Spacing();
    auto applyHudSetting = [&](const char *key, bool value, auto setter) {
        if (!bz::config::ConfigStore::Set(key, value)) {
            settingsStatusText = "Failed to save HUD settings.";
            settingsStatusIsError = true;
            return;
        }
        setter(value, false);
    };
    bool scoreboardVisible = hudSettings.scoreboardVisible();
    if (DrawOnOffToggle("Scoreboard", scoreboardVisible)) {
        applyHudSetting("ui.hud.scoreboard", scoreboardVisible,
                        [&](bool v, bool fromUser) { hudSettings.setScoreboardVisible(v, fromUser); });
    }
    bool chatVisible = hudSettings.chatVisible();
    if (DrawOnOffToggle("Chat", chatVisible)) {
        applyHudSetting("ui.hud.chat", chatVisible,
                        [&](bool v, bool fromUser) { hudSettings.setChatVisible(v, fromUser); });
    }
    bool radarVisible = hudSettings.radarVisible();
    if (DrawOnOffToggle("Radar", radarVisible)) {
        applyHudSetting("ui.hud.radar", radarVisible,
                        [&](bool v, bool fromUser) { hudSettings.setRadarVisible(v, fromUser); });
    }
    bool fpsVisible = hudSettings.fpsVisible();
    if (DrawOnOffToggle("FPS", fpsVisible)) {
        applyHudSetting("ui.hud.fps", fpsVisible,
                        [&](bool v, bool fromUser) { hudSettings.setFpsVisible(v, fromUser); });
    }
    bool crosshairVisible = hudSettings.crosshairVisible();
    if (DrawOnOffToggle("Crosshair", crosshairVisible)) {
        applyHudSetting("ui.hud.crosshair", crosshairVisible,
                        [&](bool v, bool fromUser) { hudSettings.setCrosshairVisible(v, fromUser); });
    }
    ImGui::Separator();
    ImGui::Spacing();

    if (!settingsStatusText.empty()) {
        ImVec4 statusColor = settingsStatusIsError ? colors.error : colors.notice;
        ImGui::TextColored(statusColor, "%s", settingsStatusText.c_str());
    }

}

float ConsoleView::getRenderBrightness() const {
    return renderSettings.brightness();
}

void ConsoleView::applyRenderBrightness(float value, bool fromUser) {
    renderSettings.setBrightness(value, fromUser);
}

bool ConsoleView::commitRenderBrightness() {
    if (!bz::config::ConfigStore::Set("render.brightness", renderSettings.brightness())) {
        settingsStatusText = "Failed to save render settings.";
        settingsStatusIsError = true;
        return false;
    }
    renderSettings.clearDirty();
    return true;
}

bool ConsoleView::isRenderBrightnessDragActive() const {
    return renderBrightnessDragging;
}

} // namespace ui
