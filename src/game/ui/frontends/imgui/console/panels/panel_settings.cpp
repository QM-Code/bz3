#include "ui/frontends/imgui/console/console.hpp"

#include <string>
#include <vector>

#include "common/config_store.hpp"
#include "common/i18n.hpp"
#include "spdlog/spdlog.h"
#include "ui/console/status_banner.hpp"

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
    if (settingsModel.lastConfigRevision != 0 && settingsModel.lastConfigRevision != revision) {
        spdlog::info("ImGuiSettings: config revision changed while open: {} -> {} (connected={})",
                     settingsModel.lastConfigRevision,
                     revision,
                     consoleModel.connectionState.connected);
    }
    settingsModel.lastConfigRevision = revision;

    if (!settingsModel.loaded) {
        settingsModel.loaded = true;
        settingsModel.statusText.clear();
        settingsModel.statusIsError = false;

        if (!bz::config::ConfigStore::Initialized()) {
            settingsModel.statusText = "Failed to load config; showing defaults.";
            settingsModel.statusIsError = true;
        }
        settingsModel.render.loadFromConfig();
        settingsModel.hud.loadFromConfig();
        std::string configuredLanguage = settingsController.getConfiguredLanguage();
        if (configuredLanguage.empty()) {
            configuredLanguage = bz::i18n::Get().language();
        }
        settingsModel.language = configuredLanguage;
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
                std::string error;
                if (!settingsController.setLanguage(code, &error)) {
                    settingsModel.statusText = error;
                    settingsModel.statusIsError = true;
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
    float brightness = settingsModel.render.brightness();
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
        std::string error;
        if (!settingsController.saveHudSettings(&error)) {
            settingsModel.statusText = error;
            settingsModel.statusIsError = true;
        }
    };
    bool scoreboardVisible = settingsModel.hud.scoreboardVisible();
    if (DrawOnOffToggle("Scoreboard", scoreboardVisible)) {
        applyHudSetting("ui.hud.scoreboard", scoreboardVisible,
                        [&](bool v, bool fromUser) { settingsModel.hud.setScoreboardVisible(v, fromUser); });
    }
    bool chatVisible = settingsModel.hud.chatVisible();
    if (DrawOnOffToggle("Chat", chatVisible)) {
        applyHudSetting("ui.hud.chat", chatVisible,
                        [&](bool v, bool fromUser) { settingsModel.hud.setChatVisible(v, fromUser); });
    }
    bool radarVisible = settingsModel.hud.radarVisible();
    if (DrawOnOffToggle("Radar", radarVisible)) {
        applyHudSetting("ui.hud.radar", radarVisible,
                        [&](bool v, bool fromUser) { settingsModel.hud.setRadarVisible(v, fromUser); });
    }
    bool fpsVisible = settingsModel.hud.fpsVisible();
    if (DrawOnOffToggle("FPS", fpsVisible)) {
        applyHudSetting("ui.hud.fps", fpsVisible,
                        [&](bool v, bool fromUser) { settingsModel.hud.setFpsVisible(v, fromUser); });
    }
    bool crosshairVisible = settingsModel.hud.crosshairVisible();
    if (DrawOnOffToggle("Crosshair", crosshairVisible)) {
        applyHudSetting("ui.hud.crosshair", crosshairVisible,
                        [&](bool v, bool fromUser) { settingsModel.hud.setCrosshairVisible(v, fromUser); });
    }
    ImGui::Separator();
    ImGui::Spacing();

    const auto banner = ui::status_banner::MakeStatusBanner(settingsModel.statusText,
                                                            settingsModel.statusIsError);
    if (banner.visible) {
        ImGui::Spacing();
        ImVec4 statusColor = colors.notice;
        if (banner.tone == ui::MessageTone::Error) {
            statusColor = colors.error;
        } else if (banner.tone == ui::MessageTone::Pending) {
            statusColor = colors.pending;
        }
        const std::string text = ui::status_banner::FormatStatusText(banner);
        ImGui::TextColored(statusColor, "%s", text.c_str());
        ImGui::Spacing();
    }

}

float ConsoleView::getRenderBrightness() const {
    return settingsModel.render.brightness();
}

void ConsoleView::applyRenderBrightness(float value, bool fromUser) {
    settingsModel.render.setBrightness(value, fromUser);
}

bool ConsoleView::commitRenderBrightness() {
    std::string error;
    if (!settingsController.saveRenderSettings(&error)) {
        settingsModel.statusText = error;
        settingsModel.statusIsError = true;
        return false;
    }
    settingsModel.render.clearDirty();
    return true;
}

bool ConsoleView::isRenderBrightnessDragActive() const {
    return renderBrightnessDragging;
}

} // namespace ui
