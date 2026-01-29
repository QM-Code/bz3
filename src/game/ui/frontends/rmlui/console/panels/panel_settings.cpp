#include "ui/frontends/rmlui/console/panels/panel_settings.hpp"

#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/EventListener.h>
#include <RmlUi/Core/Elements/ElementFormControlInput.h>
#include <RmlUi/Core/Elements/ElementFormControlSelect.h>

#include <cmath>
#include <sstream>
#include <string>

#include "common/config_store.hpp"
#include "common/i18n.hpp"
#include "spdlog/spdlog.h"
#include "ui/ui_config.hpp"

namespace ui {
namespace {

const std::vector<std::string> kLanguageCodes = {
    "en", "es", "fr", "de", "pt", "ru", "jp", "zh", "ko", "it", "hi", "ar"
};

std::string escapeRmlText(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    for (char ch : text) {
        switch (ch) {
            case '&': out.append("&amp;"); break;
            case '<': out.append("&lt;"); break;
            case '>': out.append("&gt;"); break;
            case '"': out.append("&quot;"); break;
            case '\'': out.append("&#39;"); break;
            default: out.push_back(ch); break;
        }
    }
    return out;
}

} // namespace

class RmlUiPanelSettings::BrightnessListener final : public Rml::EventListener {
public:
    explicit BrightnessListener(RmlUiPanelSettings *panelIn) : panel(panelIn) {}

    void ProcessEvent(Rml::Event &event) override {
        if (!panel) {
            return;
        }
        auto *target = event.GetTargetElement();
        auto *input = rmlui_dynamic_cast<Rml::ElementFormControlInput*>(target);
        if (!input) {
            return;
        }
        const std::string value = input->GetValue();
        try {
            const float brightness = std::stof(value);
            panel->applyRenderBrightness(brightness, true);
            if (event.GetType() == "change") {
                panel->setRenderBrightnessDragging(false);
                panel->commitRenderBrightness();
            } else {
                panel->setRenderBrightnessDragging(true);
            }
        } catch (...) {
            return;
        }
    }

private:
    RmlUiPanelSettings *panel = nullptr;
};

class RmlUiPanelSettings::LanguageListener final : public Rml::EventListener {
public:
    explicit LanguageListener(RmlUiPanelSettings *panelIn)
        : panel(panelIn) {}

    void ProcessEvent(Rml::Event &event) override {
        if (!panel) {
            return;
        }
        if (panel->isLanguageSelectionSuppressed()) {
            return;
        }
        auto *element = event.GetTargetElement();
        auto *select = rmlui_dynamic_cast<Rml::ElementFormControlSelect *>(element);
        if (!select) {
            return;
        }
        panel->applyLanguageSelection(select->GetValue());
    }

private:
    RmlUiPanelSettings *panel = nullptr;
};

class RmlUiPanelSettings::HudToggleListener final : public Rml::EventListener {
public:
    explicit HudToggleListener(RmlUiPanelSettings *panelIn)
        : panel(panelIn) {}

    void ProcessEvent(Rml::Event &event) override {
        if (panel) {
            panel->handleHudToggle(event.GetTargetElement());
        }
    }

private:
    RmlUiPanelSettings *panel = nullptr;
};

RmlUiPanelSettings::RmlUiPanelSettings()
    : RmlUiPanel("settings", "client/ui/console_panel_settings.rml") {}

void RmlUiPanelSettings::setUserConfigPath(const std::string &path) {
    (void)path;
    loaded = false;
    renderSettings.reset();
    hudSettings.reset();
    syncSettingsFromConfig();
    syncRenderBrightnessControls(true);
    syncHudControls();
}

void RmlUiPanelSettings::setLanguageCallback(std::function<void(const std::string &)> callback) {
    languageCallback = std::move(callback);
}

void RmlUiPanelSettings::onLoaded(Rml::ElementDocument *doc) {
    document = doc;
    if (!document) {
        return;
    }
    statusLabel = document->GetElementById("settings-status");
    brightnessSlider = document->GetElementById("settings-brightness-slider");
    brightnessValueLabel = document->GetElementById("settings-brightness-value");
    languageSelect = document->GetElementById("settings-language-select");
    hudScoreboardToggle.on = document->GetElementById("settings-hud-scoreboard-on");
    hudScoreboardToggle.off = document->GetElementById("settings-hud-scoreboard-off");
    hudChatToggle.on = document->GetElementById("settings-hud-chat-on");
    hudChatToggle.off = document->GetElementById("settings-hud-chat-off");
    hudRadarToggle.on = document->GetElementById("settings-hud-radar-on");
    hudRadarToggle.off = document->GetElementById("settings-hud-radar-off");
    hudFpsToggle.on = document->GetElementById("settings-hud-fps-on");
    hudFpsToggle.off = document->GetElementById("settings-hud-fps-off");
    hudCrosshairToggle.on = document->GetElementById("settings-hud-crosshair-on");
    hudCrosshairToggle.off = document->GetElementById("settings-hud-crosshair-off");

    listeners.clear();
    if (brightnessSlider) {
        auto listener = std::make_unique<BrightnessListener>(this);
        brightnessSlider->AddEventListener("change", listener.get());
        brightnessSlider->AddEventListener("input", listener.get());
        listeners.emplace_back(std::move(listener));
    }
    if (languageSelect) {
        auto listener = std::make_unique<LanguageListener>(this);
        languageSelect->AddEventListener("change", listener.get());
        listeners.emplace_back(std::move(listener));
        rebuildLanguageOptions();
    }
    if (hudScoreboardToggle.on || hudScoreboardToggle.off ||
        hudChatToggle.on || hudChatToggle.off ||
        hudRadarToggle.on || hudRadarToggle.off ||
        hudFpsToggle.on || hudFpsToggle.off ||
        hudCrosshairToggle.on || hudCrosshairToggle.off) {
        auto listener = std::make_unique<HudToggleListener>(this);
        if (hudScoreboardToggle.on) {
            hudScoreboardToggle.on->AddEventListener("click", listener.get());
        }
        if (hudScoreboardToggle.off) {
            hudScoreboardToggle.off->AddEventListener("click", listener.get());
        }
        if (hudChatToggle.on) {
            hudChatToggle.on->AddEventListener("click", listener.get());
        }
        if (hudChatToggle.off) {
            hudChatToggle.off->AddEventListener("click", listener.get());
        }
        if (hudRadarToggle.on) {
            hudRadarToggle.on->AddEventListener("click", listener.get());
        }
        if (hudRadarToggle.off) {
            hudRadarToggle.off->AddEventListener("click", listener.get());
        }
        if (hudFpsToggle.on) {
            hudFpsToggle.on->AddEventListener("click", listener.get());
        }
        if (hudFpsToggle.off) {
            hudFpsToggle.off->AddEventListener("click", listener.get());
        }
        if (hudCrosshairToggle.on) {
            hudCrosshairToggle.on->AddEventListener("click", listener.get());
        }
        if (hudCrosshairToggle.off) {
            hudCrosshairToggle.off->AddEventListener("click", listener.get());
        }
        listeners.emplace_back(std::move(listener));
    }

    syncSettingsFromConfig();
    syncRenderBrightnessControls(true);
    syncHudControls();
    updateStatus();
}

void RmlUiPanelSettings::onUpdate() {
    if (!document) {
        return;
    }
    if (!loaded) {
        loaded = true;
        syncSettingsFromConfig();
        syncRenderBrightnessControls(true);
        syncHudControls();
        updateStatus();
    }
    if (hudSettings.consumeDirty()) {
        if (!hudSettings.saveToConfig()) {
            showStatus("Failed to save HUD settings.", true);
        }
    }
}

void RmlUiPanelSettings::rebuildLanguageOptions() {
    if (!languageSelect) {
        return;
    }
    auto *select = rmlui_dynamic_cast<Rml::ElementFormControlSelect *>(languageSelect);
    if (!select) {
        return;
    }
    suppressLanguageSelection = true;
    select->RemoveAll();
    for (const auto &code : kLanguageCodes) {
        const std::string labelKey = "languages." + code;
        const std::string &label = bz::i18n::Get().get(labelKey);
        select->Add(label.empty() ? code : label, code);
    }
    const std::string selected = selectedLanguageFromConfig();
    for (std::size_t i = 0; i < kLanguageCodes.size(); ++i) {
        if (kLanguageCodes[i] == selected) {
            select->SetSelection(static_cast<int>(i));
            break;
        }
    }
    suppressLanguageSelection = false;
}

void RmlUiPanelSettings::applyLanguageSelection(const std::string &code) {
    if (code.empty()) {
        return;
    }
    if (code == selectedLanguageFromConfig() && code == bz::i18n::Get().language()) {
        return;
    }
    if (!ui::UiConfig::SetLanguage(code)) {
        showStatus("Failed to save language.", true);
        return;
    }
    if (languageCallback) {
        languageCallback(code);
    }
}

std::string RmlUiPanelSettings::selectedLanguageFromConfig() const {
    const std::string configured = ui::UiConfig::GetLanguage();
    if (!configured.empty()) {
        return configured;
    }
    return bz::i18n::Get().language();
}

float RmlUiPanelSettings::getRenderBrightness() const {
    return renderSettings.brightness();
}

bool RmlUiPanelSettings::isRenderBrightnessDragActive() const {
    return renderBrightnessDragging;
}

void RmlUiPanelSettings::clearRenderBrightnessDrag() {
    renderBrightnessDragging = false;
}

void RmlUiPanelSettings::applyRenderBrightness(float value, bool fromUser) {
    if (!renderSettings.setBrightness(value, fromUser)) {
        return;
    }
    if (fromUser) {
        syncRenderBrightnessLabel();
    } else {
        syncRenderBrightnessControls(true);
    }
}

bool RmlUiPanelSettings::commitRenderBrightness() {
    if (!renderSettings.saveToConfig()) {
        showStatus("Failed to save render settings.", true);
        return false;
    }
    renderSettings.clearDirty();
    return true;
}

void RmlUiPanelSettings::setRenderBrightnessDragging(bool dragging) {
    renderBrightnessDragging = dragging;
}

void RmlUiPanelSettings::syncRenderBrightnessControls(bool syncSlider) {
    if (syncSlider && brightnessSlider) {
        auto *input = rmlui_dynamic_cast<Rml::ElementFormControlInput*>(brightnessSlider);
        if (input) {
            input->SetValue(std::to_string(renderSettings.brightness()));
        }
    }
    syncRenderBrightnessLabel();
}

void RmlUiPanelSettings::syncRenderBrightnessLabel() {
    if (!brightnessValueLabel) {
        return;
    }
    std::ostringstream oss;
    oss.setf(std::ios::fixed);
    oss.precision(2);
    oss << renderSettings.brightness() << "x";
    brightnessValueLabel->SetInnerRML(oss.str());
}

void RmlUiPanelSettings::syncHudControls() {
    auto applyToggle = [](const HudToggleButtons &toggle, bool value) {
        if (toggle.on) {
            toggle.on->SetClass("active", value);
        }
        if (toggle.off) {
            toggle.off->SetClass("active", !value);
        }
    };
    applyToggle(hudScoreboardToggle, hudSettings.scoreboardVisible());
    applyToggle(hudChatToggle, hudSettings.chatVisible());
    applyToggle(hudRadarToggle, hudSettings.radarVisible());
    applyToggle(hudFpsToggle, hudSettings.fpsVisible());
    applyToggle(hudCrosshairToggle, hudSettings.crosshairVisible());
}

void RmlUiPanelSettings::handleHudToggle(Rml::Element *target) {
    if (!target) {
        return;
    }
    if (target == hudScoreboardToggle.on) {
        hudSettings.setScoreboardVisible(true, true);
    } else if (target == hudScoreboardToggle.off) {
        hudSettings.setScoreboardVisible(false, true);
    } else if (target == hudChatToggle.on) {
        hudSettings.setChatVisible(true, true);
    } else if (target == hudChatToggle.off) {
        hudSettings.setChatVisible(false, true);
    } else if (target == hudRadarToggle.on) {
        hudSettings.setRadarVisible(true, true);
    } else if (target == hudRadarToggle.off) {
        hudSettings.setRadarVisible(false, true);
    } else if (target == hudFpsToggle.on) {
        hudSettings.setFpsVisible(true, true);
    } else if (target == hudFpsToggle.off) {
        hudSettings.setFpsVisible(false, true);
    } else if (target == hudCrosshairToggle.on) {
        hudSettings.setCrosshairVisible(true, true);
    } else if (target == hudCrosshairToggle.off) {
        hudSettings.setCrosshairVisible(false, true);
    } else {
        return;
    }
    syncHudControls();
}

void RmlUiPanelSettings::showStatus(const std::string &message, bool isError) {
    statusText = message;
    statusIsError = isError;
    updateStatus();
}

void RmlUiPanelSettings::updateStatus() {
    if (!statusLabel) {
        return;
    }
    if (statusText.empty()) {
        statusLabel->SetClass("hidden", true);
        return;
    }
    statusLabel->SetClass("hidden", false);
    statusLabel->SetClass("status-error", statusIsError);
    statusLabel->SetInnerRML(escapeRmlText(statusText));
}

void RmlUiPanelSettings::syncSettingsFromConfig() {
    if (!bz::config::ConfigStore::Initialized()) {
        return;
    }

    renderSettings.loadFromConfig();
    hudSettings.loadFromConfig();
}

} // namespace ui
