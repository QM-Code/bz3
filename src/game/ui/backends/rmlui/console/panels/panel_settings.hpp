#pragma once

#include <optional>
#include <string>
#include <vector>

#include "common/json.hpp"

namespace Rml {
class Element;
class ElementDocument;
class EventListener;
}

#include "ui/backends/rmlui/console/panels/panel.hpp"

namespace ui {

class RmlUiPanelSettings final : public RmlUiPanel {
public:
    RmlUiPanelSettings();
    void setUserConfigPath(const std::string &path);
    bool consumeKeybindingsReloadRequest();
    float getRenderBrightness() const;

protected:
    void onLoaded(Rml::ElementDocument *document) override;
    void onUpdate() override;

private:
    enum class BindingColumn {
        Keyboard = 0,
        Mouse = 1,
        Controller = 2
    };

    struct BindingRow {
        Rml::Element *action = nullptr;
        Rml::Element *keyboard = nullptr;
        Rml::Element *mouse = nullptr;
        Rml::Element *controller = nullptr;
    };

    class BindingCellListener;
    class SettingsActionListener;
    class SettingsKeyListener;
    class SettingsMouseListener;
    class BrightnessListener;

    void loadBindings();
    void rebuildBindings();
    void updateSelectedLabel();
    void updateStatus();
    void setSelected(int index, BindingColumn column);
    void clearSelected();
    void saveBindings();
    void resetBindings();
    void loadRenderSettings(const bz::json::Value &userConfig);
    void saveRenderSettings(bz::json::Value &userConfig) const;
    void setRenderBrightness(float value, bool fromUser);
    void syncRenderBrightnessControls();
    void showStatus(const std::string &message, bool isError);
    void requestKeybindingsReload();
    void captureKey(int keyIdentifier);
    void captureMouse(int button);
    std::string keyIdentifierToName(int keyIdentifier) const;

    bool loadUserConfig(bz::json::Value &out) const;
    bool saveUserConfig(const bz::json::Value &userConfig, std::string &error) const;
    void setNestedConfig(bz::json::Value &root,
                         std::initializer_list<const char*> path,
                         bz::json::Value value) const;
    void eraseNestedConfig(bz::json::Value &root,
                           std::initializer_list<const char*> path) const;

    Rml::ElementDocument *document = nullptr;
    Rml::Element *bindingsList = nullptr;
    Rml::Element *selectedLabel = nullptr;
    Rml::Element *statusLabel = nullptr;
    Rml::Element *clearButton = nullptr;
    Rml::Element *saveButton = nullptr;
    Rml::Element *resetButton = nullptr;

    std::vector<BindingRow> rows;
    std::vector<std::string> keyboardBindings;
    std::vector<std::string> mouseBindings;
    std::vector<std::string> controllerBindings;
    int selectedIndex = -1;
    BindingColumn selectedColumn = BindingColumn::Keyboard;
    bool selectionJustChanged = false;
    bool loaded = false;
    bool statusIsError = false;
    bool keybindingsReloadRequested = false;
    std::string statusText;
    std::string userConfigPath;
    std::vector<std::unique_ptr<Rml::EventListener>> listeners;
    std::vector<std::unique_ptr<Rml::EventListener>> rowListeners;

    Rml::Element *brightnessSlider = nullptr;
    Rml::Element *brightnessValueLabel = nullptr;
    float renderBrightness = 1.0f;
    bool renderBrightnessDirty = false;
};

} // namespace ui
