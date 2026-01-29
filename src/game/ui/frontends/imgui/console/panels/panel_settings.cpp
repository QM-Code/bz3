#include "ui/frontends/imgui/console/console.hpp"

#include <string>
#include <vector>

#include "common/config_store.hpp"
#include "common/i18n.hpp"
#include "spdlog/spdlog.h"
#include "ui/ui_config.hpp"

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
    if (settingsLastConfigRevision != 0 && settingsLastConfigRevision != revision) {
        spdlog::info("ImGuiSettings: config revision changed while open: {} -> {} (connected={})",
                     settingsLastConfigRevision,
                     revision,
                     connectionState.connected);
    }
    settingsLastConfigRevision = revision;

    if (!settingsLoaded) {
        settingsLoaded = true;
        settingsStatusText.clear();
        settingsStatusIsError = false;

        if (!bz::config::ConfigStore::Initialized()) {
            settingsStatusText = "Failed to load config; showing defaults.";
            settingsStatusIsError = true;
        }
        renderSettings.loadFromConfig();
        hudSettings.loadFromConfig();
        std::string configuredLanguage = ui::UiConfig::GetLanguage();
        if (configuredLanguage.empty()) {
            configuredLanguage = bz::i18n::Get().language();
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
                if (!ui::UiConfig::SetLanguage(code)) {
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
    if (ImGui::SliderFloat("Brightness", &brightness, 1.0f, 3.0f, "%.2fx")) {
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
    auto applyHudSetting = [&](const char *name, bool value, auto setter) {
        setter(value, false);
        if (!hudSettings.saveToConfig()) {
            settingsStatusText = "Failed to save HUD settings.";
            settingsStatusIsError = true;
        }
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
    if (!renderSettings.saveToConfig()) {
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
