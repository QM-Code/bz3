#pragma once

#include <optional>
#include <string>
#include <functional>
#include <vector>

#include "ui/hud_settings.hpp"
#include "ui/render_settings.hpp"

namespace Rml {
class Element;
class ElementDocument;
class EventListener;
}

#include "ui/frontends/rmlui/console/panels/panel.hpp"

namespace ui {

class RmlUiPanelSettings final : public RmlUiPanel {
public:
    RmlUiPanelSettings();
    void setUserConfigPath(const std::string &path);
    bool consumeKeybindingsReloadRequest();
    float getRenderBrightness() const;
    bool isRenderBrightnessDragActive() const;
    void clearRenderBrightnessDrag();
    void setLanguageCallback(std::function<void(const std::string &)> callback);

protected:
    void onLoaded(Rml::ElementDocument *document) override;
    void onUpdate() override;

private:
    class BrightnessListener;
    class LanguageListener;
    class HudToggleListener;

    void updateStatus();
    void applyRenderBrightness(float value, bool fromUser);
    bool commitRenderBrightness();
    void syncRenderBrightnessControls(bool syncSlider);
    void syncRenderBrightnessLabel();
    void syncHudControls();
    void handleHudToggle(Rml::Element *target);
    void showStatus(const std::string &message, bool isError);
    void rebuildLanguageOptions();
    void applyLanguageSelection(const std::string &code);
    std::string selectedLanguageFromConfig() const;
    bool isLanguageSelectionSuppressed() const { return suppressLanguageSelection; }
    void syncSettingsFromConfig();
    void setRenderBrightnessDragging(bool dragging);

    Rml::ElementDocument *document = nullptr;
    Rml::Element *statusLabel = nullptr;
    Rml::Element *languageSelect = nullptr;
    bool loaded = false;
    bool statusIsError = false;
    std::string statusText;
    std::vector<std::unique_ptr<Rml::EventListener>> listeners;

    Rml::Element *brightnessSlider = nullptr;
    Rml::Element *brightnessValueLabel = nullptr;
    bool renderBrightnessDragging = false;
    struct HudToggleButtons {
        Rml::Element *on = nullptr;
        Rml::Element *off = nullptr;
    };

    HudToggleButtons hudScoreboardToggle{};
    HudToggleButtons hudChatToggle{};
    HudToggleButtons hudRadarToggle{};
    HudToggleButtons hudFpsToggle{};
    HudToggleButtons hudCrosshairToggle{};
    RenderSettings renderSettings;
    HudSettings hudSettings;
    std::function<void(const std::string &)> languageCallback;
    bool suppressLanguageSelection = false;
};

} // namespace ui
