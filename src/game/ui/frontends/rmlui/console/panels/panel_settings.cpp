#include "ui/frontends/rmlui/console/panels/panel_settings.hpp"

#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/EventListener.h>
#include <RmlUi/Core/Input.h>
#include <RmlUi/Core/Elements/ElementFormControlInput.h>
#include <RmlUi/Core/Elements/ElementFormControlSelect.h>

#include <algorithm>
#include <cmath>
#include <cctype>
#include <fstream>
#include <sstream>

#include "common/json.hpp"

#include "common/i18n.hpp"
#include "game/input/bindings.hpp"
#include "spdlog/spdlog.h"
#include "common/config_store.hpp"

namespace ui {
namespace {

const std::vector<std::string> kLanguageCodes = {
    "en", "es", "fr", "de", "pt", "ru", "jp", "zh", "ko", "it", "hi", "ar"
};

struct KeybindingDefinition {
    const char *action;
    const char *label;
};

constexpr KeybindingDefinition kKeybindings[] = {
    {"moveForward", "Move Forward"},
    {"moveBackward", "Move Backward"},
    {"moveLeft", "Move Left"},
    {"moveRight", "Move Right"},
    {"jump", "Jump"},
    {"fire", "Fire"},
    {"spawn", "Spawn"},
    {"chat", "Chat"},
    {"toggleFullscreen", "Toggle Fullscreen"},
    {"escape", "Escape Menu"},
    {"quickQuit", "Quick Quit"}
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
            panel->setRenderBrightness(brightness, true);
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
    syncRenderBrightnessControls();
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
    bindingsList = document->GetElementById("bindings-list-inner");
    selectedLabel = document->GetElementById("bindings-selected");
    statusLabel = document->GetElementById("settings-status");
    clearButton = document->GetElementById("bindings-clear");
    saveButton = document->GetElementById("bindings-save");
    resetButton = document->GetElementById("bindings-reset");
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

    loadBindings();
    rebuildBindings();
    updateSelectedLabel();
    updateStatus();
    syncHudControls();
}

void RmlUiPanelSettings::onUpdate() {
    if (!document) {
        return;
    }
    if (brightnessSlider) {
        auto *input = rmlui_dynamic_cast<Rml::ElementFormControlInput*>(brightnessSlider);
        if (input) {
            try {
                const float brightness = std::stof(input->GetValue());
                if (std::abs(brightness - renderSettings.brightness()) > 0.0001f) {
                    setRenderBrightness(brightness, true);
                }
            } catch (...) {
            }
        }
    }
    if (!loaded) {
        loadBindings();
        rebuildBindings();
        updateSelectedLabel();
        updateStatus();
        syncHudControls();
    }
    if (renderSettings.consumeDirty()) {
        if (!bz::config::ConfigStore::Set("render.brightness", renderSettings.brightness())) {
            showStatus("Failed to save render settings.", true);
        }
    }
    if (hudSettings.consumeDirty()) {
        const bool ok =
            bz::config::ConfigStore::Set("ui.hud.scoreboard", hudSettings.scoreboardVisible()) &&
            bz::config::ConfigStore::Set("ui.hud.chat", hudSettings.chatVisible()) &&
            bz::config::ConfigStore::Set("ui.hud.radar", hudSettings.radarVisible()) &&
            bz::config::ConfigStore::Set("ui.hud.fps", hudSettings.fpsVisible()) &&
            bz::config::ConfigStore::Set("ui.hud.crosshair", hudSettings.crosshairVisible());
        if (!ok) {
            showStatus("Failed to save HUD settings.", true);
        }
    }
    selectionJustChanged = false;
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
    if (!bz::config::ConfigStore::Set("language", code)) {
        showStatus("Failed to save language.", true);
        return;
    }
    if (languageCallback) {
        languageCallback(code);
    }
}

std::string RmlUiPanelSettings::selectedLanguageFromConfig() const {
    if (const auto *value = bz::config::ConfigStore::Get("language")) {
        if (value->is_string()) {
            return value->get<std::string>();
        }
    }
    return bz::i18n::Get().language();
}

void RmlUiPanelSettings::loadBindings() {
    loaded = true;
    keyboardBindings.assign(std::size(kKeybindings), std::string());
    mouseBindings.assign(std::size(kKeybindings), std::string());
    controllerBindings.assign(std::size(kKeybindings), std::string());

    renderSettings.reset();
    hudSettings.reset();
    if (!bz::config::ConfigStore::Initialized()) {
        showStatus("Failed to load config; showing defaults.", true);
    }
    syncSettingsFromConfig();
    syncRenderBrightnessControls();

    const bz::json::Value *bindingsNode = nullptr;
    if (const auto *node = bz::config::ConfigStore::Get("keybindings")) {
        if (node->is_object()) {
            bindingsNode = node;
        }
    }
    const bz::json::Value *controllerNode = nullptr;
    if (const auto *node = bz::config::ConfigStore::Get("gui.keybindings.controller")) {
        if (node->is_object()) {
            controllerNode = node;
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
            const auto &defaults = defaultBindingsForAction(kKeybindings[i].action);
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
    bz::json::Value keybindings = bz::json::Object();
    bz::json::Value controllerJson = bz::json::Object();
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
        if (!bz::config::ConfigStore::Set("keybindings", keybindings)) {
            showStatus("Failed to save bindings.", true);
            return;
        }
    } else {
        bz::config::ConfigStore::Erase("keybindings");
    }

    if (hasController) {
        if (!bz::config::ConfigStore::Set("gui.keybindings.controller", controllerJson)) {
            showStatus("Failed to save bindings.", true);
            return;
        }
    } else {
        bz::config::ConfigStore::Erase("gui.keybindings.controller");
    }
    if (!bz::config::ConfigStore::Set("render.brightness", renderSettings.brightness())) {
        showStatus("Failed to save render settings.", true);
        return;
    }
    requestKeybindingsReload();
    showStatus("Bindings saved.", false);
}

void RmlUiPanelSettings::resetBindings() {
    for (std::size_t i = 0; i < std::size(kKeybindings); ++i) {
        std::vector<std::string> keyboardEntries;
        std::vector<std::string> mouseEntries;
        const auto &defaults = defaultBindingsForAction(kKeybindings[i].action);
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

    bz::config::ConfigStore::Erase("keybindings");
    bz::config::ConfigStore::Erase("gui.keybindings.controller");
    bz::config::ConfigStore::Erase("render.brightness");
    requestKeybindingsReload();
    showStatus("Bindings reset to defaults.", false);

    rebuildBindings();
    renderSettings.reset();
    syncSettingsFromConfig();
    syncRenderBrightnessControls();
}

float RmlUiPanelSettings::getRenderBrightness() const {
    return renderSettings.brightness();
}

void RmlUiPanelSettings::setRenderBrightness(float value, bool fromUser) {
    if (!renderSettings.setBrightness(value, fromUser)) {
        return;
    }
    syncRenderBrightnessControls();
}

void RmlUiPanelSettings::syncRenderBrightnessControls() {
    if (brightnessSlider) {
        auto *input = rmlui_dynamic_cast<Rml::ElementFormControlInput*>(brightnessSlider);
        if (input) {
            input->SetValue(std::to_string(renderSettings.brightness()));
        }
    }
    if (brightnessValueLabel) {
        std::ostringstream oss;
        oss.setf(std::ios::fixed);
        oss.precision(2);
        oss << renderSettings.brightness() << "x";
        brightnessValueLabel->SetInnerRML(oss.str());
    }
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

void RmlUiPanelSettings::requestKeybindingsReload() {
    keybindingsReloadRequested = true;
}

bool RmlUiPanelSettings::consumeKeybindingsReloadRequest() {
    const bool requested = keybindingsReloadRequested;
    keybindingsReloadRequested = false;
    return requested;
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

void RmlUiPanelSettings::syncSettingsFromConfig() {
    if (!bz::config::ConfigStore::Initialized()) {
        return;
    }

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
}

} // namespace ui
