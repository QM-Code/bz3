#include "engine/components/gui/rmlui_panels/rmlui_panel_settings.hpp"

#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/EventListener.h>
#include <RmlUi/Core/Input.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>

#include <nlohmann/json.hpp>

#include "common/data_path_resolver.hpp"
#include "spdlog/spdlog.h"

namespace gui {
namespace {

struct KeybindingDefinition {
    const char *action;
    const char *label;
    const char *defaults;
};

constexpr KeybindingDefinition kKeybindings[] = {
    {"moveForward", "Move Forward", "UP, I"},
    {"moveBackward", "Move Backward", "DOWN, K"},
    {"moveLeft", "Move Left", "LEFT, J"},
    {"moveRight", "Move Right", "RIGHT, L"},
    {"jump", "Jump", "SPACE"},
    {"fire", "Fire", "F, E, LEFT_MOUSE"},
    {"spawn", "Spawn", "U"},
    {"chat", "Chat", "T"},
    {"toggleFullscreen", "Toggle Fullscreen", "RIGHT_BRACKET"},
    {"escape", "Escape Menu", "ESCAPE"},
    {"quickQuit", "Quick Quit", "F12"}
};

bool isMouseBindingName(const std::string &name) {
    std::string upper;
    upper.reserve(name.size());
    for (const char ch : name) {
        upper.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(ch))));
    }
    if (upper.rfind("MOUSE", 0) == 0) {
        return true;
    }
    if (upper.size() >= 6 && upper.substr(upper.size() - 6) == "_MOUSE") {
        return true;
    }
    return false;
}

std::string trimCopy(const std::string &text) {
    auto begin = text.begin();
    auto end = text.end();
    while (begin != end && std::isspace(static_cast<unsigned char>(*begin))) {
        ++begin;
    }
    while (end != begin && std::isspace(static_cast<unsigned char>(*(end - 1)))) {
        --end;
    }
    return std::string(begin, end);
}

std::vector<std::string> splitKeyList(const std::string &text) {
    std::vector<std::string> entries;
    std::stringstream stream(text);
    std::string token;
    while (std::getline(stream, token, ',')) {
        std::string trimmed = trimCopy(token);
        if (!trimmed.empty()) {
            entries.push_back(trimmed);
        }
    }
    return entries;
}

std::string joinEntries(const std::vector<std::string> &entries) {
    std::ostringstream oss;
    bool first = true;
    for (const auto &item : entries) {
        if (!first) {
            oss << ", ";
        }
        oss << item;
        first = false;
    }
    return oss.str();
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
            default: out.push_back(ch); break;
        }
    }
    return out;
}

} // namespace

class RmlUiPanelSettings::BindingCellListener final : public Rml::EventListener {
public:
    BindingCellListener(RmlUiPanelSettings *panelIn, int rowIndexIn, BindingColumn columnIn)
        : panel(panelIn), rowIndex(rowIndexIn), column(columnIn) {}

    void ProcessEvent(Rml::Event &) override {
        if (panel) {
            panel->setSelected(rowIndex, column);
        }
    }

private:
    RmlUiPanelSettings *panel = nullptr;
    int rowIndex = -1;
    BindingColumn column = BindingColumn::Keyboard;
};

class RmlUiPanelSettings::SettingsActionListener final : public Rml::EventListener {
public:
    enum class Action {
        Clear,
        Save,
        Reset
    };

    SettingsActionListener(RmlUiPanelSettings *panelIn, Action actionIn)
        : panel(panelIn), action(actionIn) {}

    void ProcessEvent(Rml::Event &) override {
        if (!panel) {
            return;
        }
        switch (action) {
            case Action::Clear:
                panel->clearSelected();
                break;
            case Action::Save:
                panel->saveBindings();
                break;
            case Action::Reset:
                panel->resetBindings();
                break;
        }
    }

private:
    RmlUiPanelSettings *panel = nullptr;
    Action action;
};

class RmlUiPanelSettings::SettingsKeyListener final : public Rml::EventListener {
public:
    explicit SettingsKeyListener(RmlUiPanelSettings *panelIn)
        : panel(panelIn) {}

    void ProcessEvent(Rml::Event &event) override {
        if (!panel) {
            return;
        }
        const int keyIdentifier = event.GetParameter<int>("key_identifier", Rml::Input::KI_UNKNOWN);
        panel->captureKey(keyIdentifier);
    }

private:
    RmlUiPanelSettings *panel = nullptr;
};

class RmlUiPanelSettings::SettingsMouseListener final : public Rml::EventListener {
public:
    explicit SettingsMouseListener(RmlUiPanelSettings *panelIn)
        : panel(panelIn) {}

    void ProcessEvent(Rml::Event &event) override {
        if (!panel) {
            return;
        }
        const int button = event.GetParameter<int>("button", -1);
        panel->captureMouse(button);
    }

private:
    RmlUiPanelSettings *panel = nullptr;
};

RmlUiPanelSettings::RmlUiPanelSettings()
    : RmlUiPanel("settings", "client/ui/rmlui_panel_settings.rml") {}

void RmlUiPanelSettings::setUserConfigPath(const std::string &path) {
    userConfigPath = path;
    loaded = false;
}

void RmlUiPanelSettings::onLoaded(Rml::ElementDocument *doc) {
    document = doc;
    if (!document) {
        return;
    }
    bindingsList = document->GetElementById("bindings-list-inner");
    selectedLabel = document->GetElementById("bindings-selected");
    statusLabel = document->GetElementById("settings-status");
    clearButton = document->GetElementById("bindings-clear");
    saveButton = document->GetElementById("bindings-save");
    resetButton = document->GetElementById("bindings-reset");

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

    loadBindings();
    rebuildBindings();
    updateSelectedLabel();
    updateStatus();
}

void RmlUiPanelSettings::onUpdate() {
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

void RmlUiPanelSettings::loadBindings() {
    loaded = true;
    keyboardBindings.assign(std::size(kKeybindings), std::string());
    mouseBindings.assign(std::size(kKeybindings), std::string());
    controllerBindings.assign(std::size(kKeybindings), std::string());

    nlohmann::json userConfig;
    bool loadedConfig = loadUserConfig(userConfig);
    if (!loadedConfig) {
        showStatus("Failed to load user config; showing defaults.", true);
    }

    const nlohmann::json *bindingsNode = nullptr;
    if (auto it = userConfig.find("keybindings"); it != userConfig.end() && it->is_object()) {
        bindingsNode = &(*it);
    }
    const nlohmann::json *controllerNode = nullptr;
    if (auto guiIt = userConfig.find("gui"); guiIt != userConfig.end() && guiIt->is_object()) {
        if (auto keyIt = guiIt->find("keybindings"); keyIt != guiIt->end() && keyIt->is_object()) {
            if (auto controllerIt = keyIt->find("controller"); controllerIt != keyIt->end() && controllerIt->is_object()) {
                controllerNode = &(*controllerIt);
            }
        }
    }

    for (std::size_t i = 0; i < std::size(kKeybindings); ++i) {
        std::vector<std::string> keyboardEntries;
        std::vector<std::string> mouseEntries;
        std::vector<std::string> controllerEntries;

        if (bindingsNode) {
            auto it = bindingsNode->find(kKeybindings[i].action);
            if (it != bindingsNode->end() && it->is_array()) {
                for (const auto &entry : *it) {
                    if (!entry.is_string()) {
                        continue;
                    }
                    const auto value = entry.get<std::string>();
                    if (isMouseBindingName(value)) {
                        mouseEntries.push_back(value);
                    } else {
                        keyboardEntries.push_back(value);
                    }
                }
            }
        }

        if (keyboardEntries.empty() && mouseEntries.empty()) {
            const std::vector<std::string> defaults = splitKeyList(kKeybindings[i].defaults);
            for (const auto &value : defaults) {
                if (isMouseBindingName(value)) {
                    mouseEntries.push_back(value);
                } else {
                    keyboardEntries.push_back(value);
                }
            }
        }

        if (controllerNode) {
            auto it = controllerNode->find(kKeybindings[i].action);
            if (it != controllerNode->end() && it->is_array()) {
                for (const auto &entry : *it) {
                    if (entry.is_string()) {
                        controllerEntries.push_back(entry.get<std::string>());
                    }
                }
            }
        }

        keyboardBindings[i] = joinEntries(keyboardEntries);
        mouseBindings[i] = joinEntries(mouseEntries);
        controllerBindings[i] = joinEntries(controllerEntries);
    }
}

void RmlUiPanelSettings::rebuildBindings() {
    if (!bindingsList || !document) {
        return;
    }
    bindingsList->SetInnerRML("");
    rowListeners.clear();
    rows.clear();
    rows.reserve(std::size(kKeybindings));

    auto appendElement = [&](Rml::Element *parent, const char *tag) -> Rml::Element* {
        auto child = document->CreateElement(tag);
        Rml::Element *ptr = child.get();
        parent->AppendChild(std::move(child));
        return ptr;
    };

    for (std::size_t i = 0; i < std::size(kKeybindings); ++i) {
        auto *row = appendElement(bindingsList, "div");
        row->SetClass("bindings-row", true);

        auto *actionCell = appendElement(row, "div");
        actionCell->SetClass("bindings-cell", true);
        actionCell->SetClass("action", true);
        actionCell->SetInnerRML(escapeRmlText(kKeybindings[i].label));

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

void RmlUiPanelSettings::updateSelectedLabel() {
    if (!selectedLabel) {
        return;
    }
    std::string label = "Selected cell: None";
    if (selectedIndex >= 0 && selectedIndex < static_cast<int>(std::size(kKeybindings))) {
        const char *colName = selectedColumn == BindingColumn::Keyboard
            ? "Keyboard"
            : (selectedColumn == BindingColumn::Mouse ? "Mouse" : "Controller");
        label = std::string("Selected cell: ") + kKeybindings[selectedIndex].label + " / " + colName;
    }
    selectedLabel->SetInnerRML(escapeRmlText(label));
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

void RmlUiPanelSettings::setSelected(int index, BindingColumn column) {
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

void RmlUiPanelSettings::clearSelected() {
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

void RmlUiPanelSettings::saveBindings() {
    nlohmann::json userConfig;
    if (!loadUserConfig(userConfig)) {
        showStatus("Failed to load user config.", true);
        return;
    }

    nlohmann::json keybindings = nlohmann::json::object();
    nlohmann::json controllerJson = nlohmann::json::object();
    bool hasBindings = false;
    bool hasController = false;

    for (std::size_t i = 0; i < std::size(kKeybindings); ++i) {
        const std::vector<std::string> keyboardValues = splitKeyList(keyboardBindings[i]);
        const std::vector<std::string> mouseValues = splitKeyList(mouseBindings[i]);
        const std::vector<std::string> controllerValues = splitKeyList(controllerBindings[i]);

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
            keybindings[kKeybindings[i].action] = combined;
            hasBindings = true;
        }

        if (!controllerValues.empty()) {
            controllerJson[kKeybindings[i].action] = controllerValues;
            hasController = true;
        }
    }

    if (hasBindings) {
        setNestedConfig(userConfig, {"keybindings"}, keybindings);
    } else {
        eraseNestedConfig(userConfig, {"keybindings"});
    }

    if (hasController) {
        setNestedConfig(userConfig, {"gui", "keybindings", "controller"}, controllerJson);
    } else {
        eraseNestedConfig(userConfig, {"gui", "keybindings", "controller"});
    }

    std::string error;
    if (!saveUserConfig(userConfig, error)) {
        showStatus(error.empty() ? "Failed to save bindings." : error, true);
    } else {
        showStatus("Bindings saved. Restart to apply.", false);
    }
}

void RmlUiPanelSettings::resetBindings() {
    for (std::size_t i = 0; i < std::size(kKeybindings); ++i) {
        std::vector<std::string> keyboardEntries;
        std::vector<std::string> mouseEntries;
        const std::vector<std::string> defaults = splitKeyList(kKeybindings[i].defaults);
        for (const auto &value : defaults) {
            if (isMouseBindingName(value)) {
                mouseEntries.push_back(value);
            } else {
                keyboardEntries.push_back(value);
            }
        }
        keyboardBindings[i] = joinEntries(keyboardEntries);
        mouseBindings[i] = joinEntries(mouseEntries);
        controllerBindings[i].clear();
    }

    nlohmann::json userConfig;
    if (!loadUserConfig(userConfig)) {
        showStatus("Failed to load user config.", true);
    } else {
        eraseNestedConfig(userConfig, {"keybindings"});
        eraseNestedConfig(userConfig, {"gui", "keybindings", "controller"});
        std::string error;
        if (!saveUserConfig(userConfig, error)) {
            showStatus(error.empty() ? "Failed to reset bindings." : error, true);
        } else {
            showStatus("Bindings reset to defaults. Restart to apply.", false);
        }
    }

    rebuildBindings();
}

void RmlUiPanelSettings::showStatus(const std::string &message, bool isError) {
    statusText = message;
    statusIsError = isError;
    updateStatus();
}

void RmlUiPanelSettings::captureKey(int keyIdentifier) {
    if (selectedIndex < 0 || selectedIndex >= static_cast<int>(keyboardBindings.size())) {
        return;
    }
    if (keyIdentifier == Rml::Input::KI_UNKNOWN) {
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
        auto entries = splitKeyList(keyboardBindings[selectedIndex]);
        if (std::find(entries.begin(), entries.end(), name) == entries.end()) {
            entries.push_back(name);
            keyboardBindings[selectedIndex] = joinEntries(entries);
            rebuildBindings();
        }
    } else {
        auto entries = splitKeyList(controllerBindings[selectedIndex]);
        if (std::find(entries.begin(), entries.end(), name) == entries.end()) {
            entries.push_back(name);
            controllerBindings[selectedIndex] = joinEntries(entries);
            rebuildBindings();
        }
    }
}

void RmlUiPanelSettings::captureMouse(int button) {
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
    auto entries = splitKeyList(mouseBindings[selectedIndex]);
    if (std::find(entries.begin(), entries.end(), name) == entries.end()) {
        entries.push_back(name);
        mouseBindings[selectedIndex] = joinEntries(entries);
        rebuildBindings();
    }
}

std::string RmlUiPanelSettings::keyIdentifierToName(int keyIdentifier) const {
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

    switch (keyIdentifier) {
        case Rml::Input::KI_SPACE: return "SPACE";
        case Rml::Input::KI_RETURN: return "ENTER";
        case Rml::Input::KI_ESCAPE: return "ESCAPE";
        case Rml::Input::KI_TAB: return "TAB";
        case Rml::Input::KI_BACK: return "BACKSPACE";
        case Rml::Input::KI_LEFT: return "LEFT";
        case Rml::Input::KI_RIGHT: return "RIGHT";
        case Rml::Input::KI_UP: return "UP";
        case Rml::Input::KI_DOWN: return "DOWN";
        case Rml::Input::KI_HOME: return "HOME";
        case Rml::Input::KI_END: return "END";
        case Rml::Input::KI_PRIOR: return "PAGE_UP";
        case Rml::Input::KI_NEXT: return "PAGE_DOWN";
        case Rml::Input::KI_INSERT: return "INSERT";
        case Rml::Input::KI_DELETE: return "DELETE";
        case Rml::Input::KI_CAPITAL: return "CAPS_LOCK";
        case Rml::Input::KI_NUMLOCK: return "NUM_LOCK";
        case Rml::Input::KI_SCROLL: return "SCROLL_LOCK";
        case Rml::Input::KI_LSHIFT: return "LEFT_SHIFT";
        case Rml::Input::KI_RSHIFT: return "RIGHT_SHIFT";
        case Rml::Input::KI_LCONTROL: return "LEFT_CONTROL";
        case Rml::Input::KI_RCONTROL: return "RIGHT_CONTROL";
        case Rml::Input::KI_LMENU: return "LEFT_ALT";
        case Rml::Input::KI_LWIN: return "LEFT_SUPER";
        case Rml::Input::KI_RWIN: return "RIGHT_SUPER";
        default: break;
    }
    return {};
}

bool RmlUiPanelSettings::loadUserConfig(nlohmann::json &out) const {
    const std::filesystem::path path = userConfigPath.empty()
        ? bz::data::EnsureUserConfigFile("config.json")
        : std::filesystem::path(userConfigPath);
    if (auto user = bz::data::LoadJsonFile(path, "user config", spdlog::level::debug)) {
        if (!user->is_object()) {
            out = nlohmann::json::object();
            return false;
        }
        out = *user;
        return true;
    }
    out = nlohmann::json::object();
    return true;
}

bool RmlUiPanelSettings::saveUserConfig(const nlohmann::json &userConfig, std::string &error) const {
    const std::filesystem::path path = userConfigPath.empty()
        ? bz::data::EnsureUserConfigFile("config.json")
        : std::filesystem::path(userConfigPath);
    std::error_code ec;
    const auto parentDir = path.parent_path();
    if (!parentDir.empty()) {
        std::filesystem::create_directories(parentDir, ec);
        if (ec) {
            error = "Failed to create config directory.";
            return false;
        }
    }

    std::ofstream file(path, std::ios::trunc);
    if (!file.is_open()) {
        error = "Failed to open user config for writing.";
        return false;
    }

    try {
        file << userConfig.dump(4) << '\n';
    } catch (const std::exception &ex) {
        error = ex.what();
        return false;
    }
    return true;
}

void RmlUiPanelSettings::setNestedConfig(nlohmann::json &root,
                                         std::initializer_list<const char*> path,
                                         nlohmann::json value) const {
    nlohmann::json *cursor = &root;
    std::vector<std::string> keys;
    keys.reserve(path.size());
    for (const char *entry : path) {
        keys.emplace_back(entry);
    }
    if (keys.empty()) {
        return;
    }
    for (std::size_t i = 0; i + 1 < keys.size(); ++i) {
        const std::string &key = keys[i];
        if (!cursor->contains(key) || !(*cursor)[key].is_object()) {
            (*cursor)[key] = nlohmann::json::object();
        }
        cursor = &(*cursor)[key];
    }
    (*cursor)[keys.back()] = std::move(value);
}

void RmlUiPanelSettings::eraseNestedConfig(nlohmann::json &root,
                                           std::initializer_list<const char*> path) const {
    nlohmann::json *cursor = &root;
    std::vector<std::string> keys;
    keys.reserve(path.size());
    for (const char *entry : path) {
        keys.emplace_back(entry);
    }
    if (keys.empty()) {
        return;
    }
    for (std::size_t i = 0; i + 1 < keys.size(); ++i) {
        const std::string &key = keys[i];
        if (!cursor->contains(key) || !(*cursor)[key].is_object()) {
            return;
        }
        cursor = &(*cursor)[key];
    }
    cursor->erase(keys.back());
}

} // namespace gui
