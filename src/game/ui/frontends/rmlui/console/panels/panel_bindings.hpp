#pragma once

#include <memory>
#include <string>
#include <vector>

namespace Rml {
class Element;
class ElementDocument;
class EventListener;
}

#include "ui/frontends/rmlui/console/panels/panel.hpp"

namespace ui {

class RmlUiPanelBindings final : public RmlUiPanel {
public:
    RmlUiPanelBindings();
    void setUserConfigPath(const std::string &path);
    bool consumeKeybindingsReloadRequest();

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

    void loadBindings();
    void rebuildBindings();
    void updateSelectedLabel();
    void updateStatus();
    void setSelected(int index, BindingColumn column);
    void clearSelected();
    void saveBindings();
    void resetBindings();
    void showStatus(const std::string &message, bool isError);
    void requestKeybindingsReload();
    void captureKey(int keyIdentifier);
    void captureMouse(int button);
    std::string keyIdentifierToName(int keyIdentifier) const;

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
    std::vector<std::unique_ptr<Rml::EventListener>> listeners;
    std::vector<std::unique_ptr<Rml::EventListener>> rowListeners;
};

} // namespace ui
