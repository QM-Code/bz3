#include "ui/frontends/rmlui/console/panels/panel_bindings.hpp"

#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/EventListener.h>
#include <RmlUi/Core/Input.h>

#include <algorithm>
#include <cctype>
#include <sstream>

#include "common/config_store.hpp"
#include "game/input/bindings.hpp"
#include "ui/console/keybindings.hpp"
#include "ui/ui_config.hpp"

namespace ui {
namespace {

const std::vector<std::string>& defaultBindingsForAction(std::string_view action) {
    static const game_input::DefaultBindingsMap kDefaults = game_input::DefaultKeybindings();
    static const std::vector<std::string> kEmpty;
    auto it = kDefaults.find(std::string(action));
    if (it == kDefaults.end()) {
        return kEmpty;
    }
    return it->second;
}

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

class RmlUiPanelBindings::BindingCellListener final : public Rml::EventListener {
public:
    BindingCellListener(RmlUiPanelBindings *panelIn, int rowIndexIn, BindingColumn columnIn)
        : panel(panelIn), rowIndex(rowIndexIn), column(columnIn) {}

    void ProcessEvent(Rml::Event &) override {
        if (panel) {
            panel->setSelected(rowIndex, column);
        }
    }

private:
    RmlUiPanelBindings *panel = nullptr;
    int rowIndex = 0;
    BindingColumn column = BindingColumn::Keyboard;
};

class RmlUiPanelBindings::SettingsActionListener final : public Rml::EventListener {
public:
    enum class Action {
        Clear,
        Save,
        Reset
    };

    SettingsActionListener(RmlUiPanelBindings *panelIn, Action actionIn)
        : panel(panelIn), action(actionIn) {}

    void ProcessEvent(Rml::Event &) override {
        if (!panel) {
            return;
        }
        switch (action) {
            case Action::Clear: panel->clearSelected(); break;
            case Action::Save: panel->saveBindings(); break;
            case Action::Reset: panel->showResetDialog(); break;
        }
    }

private:
    RmlUiPanelBindings *panel = nullptr;
    Action action = Action::Clear;
};

class RmlUiPanelBindings::SettingsKeyListener final : public Rml::EventListener {
public:
    explicit SettingsKeyListener(RmlUiPanelBindings *panelIn)
        : panel(panelIn) {}

    void ProcessEvent(Rml::Event &event) override {
        if (!panel) {
            return;
        }
        const int keyIdentifier = event.GetParameter<int>("key_identifier", Rml::Input::KI_UNKNOWN);
        panel->captureKey(keyIdentifier);
    }

private:
    RmlUiPanelBindings *panel = nullptr;
};

class RmlUiPanelBindings::SettingsMouseListener final : public Rml::EventListener {
public:
    explicit SettingsMouseListener(RmlUiPanelBindings *panelIn)
        : panel(panelIn) {}

    void ProcessEvent(Rml::Event &event) override {
        if (!panel) {
            return;
        }
        const int button = event.GetParameter<int>("button", -1);
        panel->handleMouseClick(event.GetTargetElement(), button);
        panel->captureMouse(button);
    }

private:
    RmlUiPanelBindings *panel = nullptr;
};

RmlUiPanelBindings::RmlUiPanelBindings()
    : RmlUiPanel("bindings", "client/ui/console_panel_bindings.rml") {}

void RmlUiPanelBindings::setUserConfigPath(const std::string &path) {
    (void)path;
    loaded = false;
    statusText.clear();
    statusIsError = false;
    keybindingsReloadRequested = false;
    selectedIndex = -1;
    selectedColumn = BindingColumn::Keyboard;
    selectionJustChanged = false;

    if (document) {
        loadBindings();
        rebuildBindings();
        updateSelectedLabel();
        updateStatus();
    }
}

bool RmlUiPanelBindings::consumeKeybindingsReloadRequest() {
    const bool requested = keybindingsReloadRequested;
    keybindingsReloadRequested = false;
    return requested;
}

void RmlUiPanelBindings::onLoaded(Rml::ElementDocument *doc) {
    document = doc;
    if (!document) {
        return;
    }
    bindingsList = document->GetElementById("bindings-list-inner");
    selectedLabel = document->GetElementById("bindings-selected");
    statusLabel = document->GetElementById("bindings-status");
    clearButton = document->GetElementById("bindings-clear");
    saveButton = document->GetElementById("bindings-save");
    resetButton = document->GetElementById("bindings-reset");
    resetDialog.bind(document,
                     "bindings-reset-overlay",
                     "bindings-reset-message",
                     "bindings-reset-yes",
                     "bindings-reset-no");
    resetDialog.setOnAccept([this]() {
        resetBindings();
        resetDialog.hide();
    });
    resetDialog.setOnCancel([this]() { resetDialog.hide(); });

    listeners.clear();
    if (clearButton) {
        auto listener = std::make_unique<SettingsActionListener>(this, SettingsActionListener::Action::Clear);
        clearButton->AddEventListener("click", listener.get());
        listeners.emplace_back(std::move(listener));
    }
    if (saveButton) {
        auto listener = std::make_unique<SettingsActionListener>(this, SettingsActionListener::Action::Save);
        saveButton->AddEventListener("click", listener.get());
        listeners.emplace_back(std::move(listener));
    }
    if (resetButton) {
        auto listener = std::make_unique<SettingsActionListener>(this, SettingsActionListener::Action::Reset);
        resetButton->AddEventListener("click", listener.get());
        listeners.emplace_back(std::move(listener));
    }
    if (document) {
        auto listener = std::make_unique<SettingsKeyListener>(this);
        document->AddEventListener("keydown", listener.get());
        listeners.emplace_back(std::move(listener));
    }
    if (document) {
        auto listener = std::make_unique<SettingsMouseListener>(this);
        document->AddEventListener("mousedown", listener.get());
        listeners.emplace_back(std::move(listener));
    }
    resetDialog.installListeners(listeners);

    loadBindings();
    rebuildBindings();
    updateSelectedLabel();
    updateStatus();
}

void RmlUiPanelBindings::onUpdate() {
    if (!document) {
        return;
    }
    if (!loaded) {
        loadBindings();
        rebuildBindings();
        updateSelectedLabel();
        updateStatus();
    }
    selectionJustChanged = false;
}

void RmlUiPanelBindings::loadBindings() {
    loaded = true;
    keyboardBindings.assign(std::size(ui::bindings::Definitions()), std::string());
    mouseBindings.assign(std::size(ui::bindings::Definitions()), std::string());
    controllerBindings.assign(std::size(ui::bindings::Definitions()), std::string());

    if (!bz::config::ConfigStore::Initialized()) {
        showStatus("Failed to load config; showing defaults.", true);
    }

    const bz::json::Value *bindingsNode = nullptr;
    auto keybindingsNode = ui::UiConfig::GetKeybindings();
    if (keybindingsNode && keybindingsNode->is_object()) {
        bindingsNode = &(*keybindingsNode);
    }
    const bz::json::Value *controllerNode = nullptr;
    auto controllerBindingsNode = ui::UiConfig::GetControllerKeybindings();
    if (controllerBindingsNode && controllerBindingsNode->is_object()) {
        controllerNode = &(*controllerBindingsNode);
    }

    auto defs = ui::bindings::Definitions();
    for (std::size_t i = 0; i < defs.size(); ++i) {
        std::vector<std::string> keyboardEntries;
        std::vector<std::string> mouseEntries;
        std::vector<std::string> controllerEntries;

        if (bindingsNode) {
            auto it = bindingsNode->find(defs[i].action);
            if (it != bindingsNode->end() && it->is_array()) {
                for (const auto &entry : *it) {
                    if (!entry.is_string()) {
                        continue;
                    }
                    const auto value = entry.get<std::string>();
                    if (ui::bindings::IsMouseBindingName(value)) {
                        mouseEntries.push_back(value);
                    } else {
                        keyboardEntries.push_back(value);
                    }
                }
            }
        }

        if (keyboardEntries.empty() && mouseEntries.empty()) {
            const auto &defaults = defaultBindingsForAction(defs[i].action);
            for (const auto &value : defaults) {
                if (ui::bindings::IsMouseBindingName(value)) {
                    mouseEntries.push_back(value);
                } else {
                    keyboardEntries.push_back(value);
                }
            }
        }

        if (controllerNode) {
            auto it = controllerNode->find(defs[i].action);
            if (it != controllerNode->end() && it->is_array()) {
                for (const auto &entry : *it) {
                    if (entry.is_string()) {
                        controllerEntries.push_back(entry.get<std::string>());
                    }
                }
            }
        }

        keyboardBindings[i] = ui::bindings::JoinBindings(keyboardEntries);
        mouseBindings[i] = ui::bindings::JoinBindings(mouseEntries);
        controllerBindings[i] = ui::bindings::JoinBindings(controllerEntries);
    }
}

void RmlUiPanelBindings::rebuildBindings() {
    if (!bindingsList || !document) {
        return;
    }
    bindingsList->SetInnerRML("");
    rowListeners.clear();
    rows.clear();
    rows.reserve(std::size(ui::bindings::Definitions()));

    auto appendElement = [&](Rml::Element *parent, const char *tag) -> Rml::Element* {
        auto child = document->CreateElement(tag);
        Rml::Element *ptr = child.get();
        parent->AppendChild(std::move(child));
        return ptr;
    };

    auto defs = ui::bindings::Definitions();
    for (std::size_t i = 0; i < defs.size(); ++i) {
        auto *row = appendElement(bindingsList, "div");
        row->SetClass("bindings-row", true);

        auto *actionCell = appendElement(row, "div");
        actionCell->SetClass("bindings-cell", true);
        actionCell->SetClass("action", true);
        actionCell->SetInnerRML(escapeRmlText(defs[i].label));

        auto makeBindingCell = [&](BindingColumn column, const std::string &value, const char *columnClass) -> Rml::Element* {
            auto *cell = appendElement(row, "div");
            cell->SetClass("bindings-cell", true);
            cell->SetClass(columnClass, true);
            auto *binding = appendElement(cell, "div");
            binding->SetClass("binding-cell", true);
            std::string display = value.empty() ? "Unbound" : value;
            binding->SetInnerRML(escapeRmlText(display));
            auto listener = std::make_unique<BindingCellListener>(this, static_cast<int>(i), column);
            binding->AddEventListener("click", listener.get());
            rowListeners.emplace_back(std::move(listener));
            return binding;
        };

        BindingRow bindingRow;
        bindingRow.action = actionCell;
        bindingRow.keyboard = makeBindingCell(BindingColumn::Keyboard, keyboardBindings[i], "keyboard");
        bindingRow.mouse = makeBindingCell(BindingColumn::Mouse, mouseBindings[i], "mouse");
        bindingRow.controller = makeBindingCell(BindingColumn::Controller, controllerBindings[i], "controller");
        rows.push_back(bindingRow);
    }
    updateSelectedLabel();
    updateStatus();
    selectionJustChanged = false;
    updateSelectedLabel();
}

void RmlUiPanelBindings::updateSelectedLabel() {
    if (!selectedLabel) {
        return;
    }
    std::string label = "Selected cell: None";
    auto defs = ui::bindings::Definitions();
    if (selectedIndex >= 0 && selectedIndex < static_cast<int>(defs.size())) {
        const char *colName = selectedColumn == BindingColumn::Keyboard
            ? "Keyboard"
            : (selectedColumn == BindingColumn::Mouse ? "Mouse" : "Controller");
        label = std::string("Selected cell: ") + defs[static_cast<std::size_t>(selectedIndex)].label + " / " + colName;
    }
    selectedLabel->SetInnerRML(escapeRmlText(label));
}

void RmlUiPanelBindings::updateStatus() {
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

void RmlUiPanelBindings::setSelected(int index, BindingColumn column) {
    selectedIndex = index;
    selectedColumn = column;
    selectionJustChanged = true;
    for (std::size_t i = 0; i < rows.size(); ++i) {
        auto setSelected = [&](Rml::Element *element, BindingColumn col) {
            if (!element) {
                return;
            }
            const bool selected = (static_cast<int>(i) == selectedIndex && col == selectedColumn);
            element->SetClass("selected", selected);
        };
        setSelected(rows[i].keyboard, BindingColumn::Keyboard);
        setSelected(rows[i].mouse, BindingColumn::Mouse);
        setSelected(rows[i].controller, BindingColumn::Controller);
    }
    updateSelectedLabel();
}

void RmlUiPanelBindings::clearSelection() {
    selectedIndex = -1;
    for (auto &row : rows) {
        if (row.keyboard) {
            row.keyboard->SetClass("selected", false);
        }
        if (row.mouse) {
            row.mouse->SetClass("selected", false);
        }
        if (row.controller) {
            row.controller->SetClass("selected", false);
        }
    }
    updateSelectedLabel();
}

void RmlUiPanelBindings::clearSelected() {
    if (selectedIndex < 0 || selectedIndex >= static_cast<int>(keyboardBindings.size())) {
        return;
    }
    if (selectedColumn == BindingColumn::Keyboard) {
        keyboardBindings[selectedIndex].clear();
    } else if (selectedColumn == BindingColumn::Mouse) {
        mouseBindings[selectedIndex].clear();
    } else {
        controllerBindings[selectedIndex].clear();
    }
    rebuildBindings();
}

void RmlUiPanelBindings::saveBindings() {
    bz::json::Value keybindings = bz::json::Object();
    bz::json::Value controllerJson = bz::json::Object();
    bool hasBindings = false;
    bool hasController = false;

    auto defs = ui::bindings::Definitions();
    for (std::size_t i = 0; i < defs.size(); ++i) {
        const std::vector<std::string> keyboardValues = ui::bindings::SplitBindings(keyboardBindings[i]);
        const std::vector<std::string> mouseValues = ui::bindings::SplitBindings(mouseBindings[i]);
        const std::vector<std::string> controllerValues = ui::bindings::SplitBindings(controllerBindings[i]);

        std::vector<std::string> combined;
        combined.reserve(keyboardValues.size() + mouseValues.size());
        for (const auto &value : keyboardValues) {
            if (!value.empty()) {
                combined.push_back(value);
            }
        }
        for (const auto &value : mouseValues) {
            if (!value.empty()) {
                combined.push_back(value);
            }
        }

        if (!combined.empty()) {
            keybindings[defs[i].action] = combined;
            hasBindings = true;
        }

        if (!controllerValues.empty()) {
            controllerJson[defs[i].action] = controllerValues;
            hasController = true;
        }
    }

    if (hasBindings) {
        if (!ui::UiConfig::SetKeybindings(keybindings)) {
            showStatus("Failed to save bindings.", true);
            return;
        }
    } else {
        ui::UiConfig::EraseKeybindings();
    }

    if (hasController) {
        if (!ui::UiConfig::SetControllerKeybindings(controllerJson)) {
            showStatus("Failed to save bindings.", true);
            return;
        }
    } else {
        ui::UiConfig::EraseControllerKeybindings();
    }
    requestKeybindingsReload();
    showStatus("Bindings saved.", false);
}

void RmlUiPanelBindings::resetBindings() {
    auto defs = ui::bindings::Definitions();
    for (std::size_t i = 0; i < defs.size(); ++i) {
        std::vector<std::string> keyboardEntries;
        std::vector<std::string> mouseEntries;
        const auto &defaults = defaultBindingsForAction(defs[i].action);
        for (const auto &value : defaults) {
            if (ui::bindings::IsMouseBindingName(value)) {
                mouseEntries.push_back(value);
            } else {
                keyboardEntries.push_back(value);
            }
        }
        keyboardBindings[i] = ui::bindings::JoinBindings(keyboardEntries);
        mouseBindings[i] = ui::bindings::JoinBindings(mouseEntries);
        controllerBindings[i].clear();
    }

    ui::UiConfig::EraseKeybindings();
    ui::UiConfig::EraseControllerKeybindings();
    requestKeybindingsReload();
    showStatus("Bindings reset to defaults.", false);

    rebuildBindings();
}

void RmlUiPanelBindings::showResetDialog() {
    resetDialog.show("Reset all keybindings to defaults? This will overwrite your custom bindings.");
}

void RmlUiPanelBindings::showStatus(const std::string &message, bool isError) {
    statusText = message;
    statusIsError = isError;
    updateStatus();
}

void RmlUiPanelBindings::requestKeybindingsReload() {
    keybindingsReloadRequested = true;
}

void RmlUiPanelBindings::captureKey(int keyIdentifier) {
    if (selectedIndex < 0 || selectedIndex >= static_cast<int>(keyboardBindings.size())) {
        return;
    }
    if (keyIdentifier == Rml::Input::KI_UNKNOWN) {
        return;
    }
    if (selectedColumn == BindingColumn::Mouse && keyIdentifier == Rml::Input::KI_ESCAPE) {
        saveBindings();
        clearSelection();
        return;
    }
    if (selectedColumn == BindingColumn::Mouse) {
        return;
    }
    const std::string name = keyIdentifierToName(keyIdentifier);
    if (name.empty()) {
        return;
    }
    if (selectedColumn == BindingColumn::Keyboard) {
        auto entries = ui::bindings::SplitBindings(keyboardBindings[selectedIndex]);
        if (std::find(entries.begin(), entries.end(), name) == entries.end()) {
            entries.push_back(name);
            keyboardBindings[selectedIndex] = ui::bindings::JoinBindings(entries);
            rebuildBindings();
        }
    } else {
        auto entries = ui::bindings::SplitBindings(controllerBindings[selectedIndex]);
        if (std::find(entries.begin(), entries.end(), name) == entries.end()) {
            entries.push_back(name);
            controllerBindings[selectedIndex] = ui::bindings::JoinBindings(entries);
            rebuildBindings();
        }
    }
}

void RmlUiPanelBindings::handleMouseClick(Rml::Element *target, int button) {
    if (button != 0) {
        return;
    }
    if (selectedIndex < 0 || selectedIndex >= static_cast<int>(rows.size())) {
        return;
    }
    if (selectedColumn != BindingColumn::Keyboard) {
        return;
    }
    if (target == clearButton || target == saveButton || target == resetButton) {
        return;
    }
    bool targetIsBindingCell = false;
    int targetIndex = -1;
    BindingColumn targetColumn = BindingColumn::Keyboard;
    for (std::size_t i = 0; i < rows.size(); ++i) {
        if (target == rows[i].keyboard) {
            targetIsBindingCell = true;
            targetIndex = static_cast<int>(i);
            targetColumn = BindingColumn::Keyboard;
            break;
        }
        if (target == rows[i].mouse) {
            targetIsBindingCell = true;
            targetIndex = static_cast<int>(i);
            targetColumn = BindingColumn::Mouse;
            break;
        }
        if (target == rows[i].controller) {
            targetIsBindingCell = true;
            targetIndex = static_cast<int>(i);
            targetColumn = BindingColumn::Controller;
            break;
        }
    }
    if (targetIsBindingCell && targetIndex == selectedIndex && targetColumn == selectedColumn) {
        return;
    }
    saveBindings();
    if (!targetIsBindingCell) {
        clearSelection();
    }
}

void RmlUiPanelBindings::captureMouse(int button) {
    if (selectionJustChanged) {
        return;
    }
    if (selectedIndex < 0 || selectedIndex >= static_cast<int>(mouseBindings.size())) {
        return;
    }
    if (selectedColumn != BindingColumn::Mouse) {
        return;
    }
    std::string name;
    switch (button) {
        case 0: name = "LEFT_MOUSE"; break;
        case 1: name = "RIGHT_MOUSE"; break;
        case 2: name = "MIDDLE_MOUSE"; break;
        case 3: name = "MOUSE4"; break;
        case 4: name = "MOUSE5"; break;
        case 5: name = "MOUSE6"; break;
        case 6: name = "MOUSE7"; break;
        case 7: name = "MOUSE8"; break;
        default: break;
    }
    if (name.empty()) {
        return;
    }
    auto entries = ui::bindings::SplitBindings(mouseBindings[selectedIndex]);
    if (std::find(entries.begin(), entries.end(), name) == entries.end()) {
        entries.push_back(name);
        mouseBindings[selectedIndex] = ui::bindings::JoinBindings(entries);
        rebuildBindings();
    }
}

std::string RmlUiPanelBindings::keyIdentifierToName(int keyIdentifier) const {
    using KeyInfo = struct { int key; const char *name; };
    static const KeyInfo keyMap[] = {
        {Rml::Input::KI_SPACE, "SPACE"},
        {Rml::Input::KI_RETURN, "ENTER"},
        {Rml::Input::KI_ESCAPE, "ESCAPE"},
        {Rml::Input::KI_TAB, "TAB"},
        {Rml::Input::KI_BACK, "BACKSPACE"},
        {Rml::Input::KI_LEFT, "LEFT"},
        {Rml::Input::KI_RIGHT, "RIGHT"},
        {Rml::Input::KI_UP, "UP"},
        {Rml::Input::KI_DOWN, "DOWN"},
        {Rml::Input::KI_OEM_4, "LEFT_BRACKET"},
        {Rml::Input::KI_OEM_6, "RIGHT_BRACKET"},
        {Rml::Input::KI_OEM_MINUS, "MINUS"},
        {Rml::Input::KI_OEM_PLUS, "EQUAL"},
        {Rml::Input::KI_OEM_7, "APOSTROPHE"},
        {Rml::Input::KI_OEM_3, "GRAVE_ACCENT"},
        {Rml::Input::KI_HOME, "HOME"},
        {Rml::Input::KI_END, "END"},
        {Rml::Input::KI_PRIOR, "PAGE_UP"},
        {Rml::Input::KI_NEXT, "PAGE_DOWN"},
        {Rml::Input::KI_INSERT, "INSERT"},
        {Rml::Input::KI_DELETE, "DELETE"},
        {Rml::Input::KI_CAPITAL, "CAPS_LOCK"},
        {Rml::Input::KI_NUMLOCK, "NUM_LOCK"},
        {Rml::Input::KI_SCROLL, "SCROLL_LOCK"},
        {Rml::Input::KI_LSHIFT, "LEFT_SHIFT"},
        {Rml::Input::KI_RSHIFT, "RIGHT_SHIFT"},
        {Rml::Input::KI_LCONTROL, "LEFT_CONTROL"},
        {Rml::Input::KI_RCONTROL, "RIGHT_CONTROL"},
        {Rml::Input::KI_LMENU, "LEFT_ALT"},
        {Rml::Input::KI_RMENU, "RIGHT_ALT"},
        {Rml::Input::KI_LMETA, "LEFT_SUPER"},
        {Rml::Input::KI_RMETA, "RIGHT_SUPER"},
        {Rml::Input::KI_UNKNOWN, "MENU"}
    };

    if (keyIdentifier >= Rml::Input::KI_A && keyIdentifier <= Rml::Input::KI_Z) {
        char name[2] = { static_cast<char>('A' + (keyIdentifier - Rml::Input::KI_A)), '\0' };
        return std::string(name);
    }
    if (keyIdentifier >= Rml::Input::KI_0 && keyIdentifier <= Rml::Input::KI_9) {
        char name[2] = { static_cast<char>('0' + (keyIdentifier - Rml::Input::KI_0)), '\0' };
        return std::string(name);
    }
    if (keyIdentifier >= Rml::Input::KI_F1 && keyIdentifier <= Rml::Input::KI_F12) {
        return "F" + std::to_string(1 + (keyIdentifier - Rml::Input::KI_F1));
    }
    for (const auto &entry : keyMap) {
        if (entry.key == keyIdentifier) {
            return std::string(entry.name);
        }
    }
    return {};
}

} // namespace ui
