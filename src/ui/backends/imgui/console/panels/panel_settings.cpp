#include "ui/backends/imgui/console/console.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "common/json.hpp"

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

bool IsMouseBindingName(const std::string &name) {
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

std::string TrimCopy(const std::string &text) {
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

std::string JoinEntries(const std::vector<std::string> &entries) {
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

std::vector<std::string> SplitKeyList(const std::string &text) {
    std::vector<std::string> entries;
    std::stringstream stream(text);
    std::string token;
    while (std::getline(stream, token, ',')) {
        std::string trimmed = TrimCopy(token);
        if (!trimmed.empty()) {
            entries.push_back(trimmed);
        }
    }
    return entries;
}

void WriteBuffer(std::array<char, 128> &buffer, const std::string &value) {
    std::snprintf(buffer.data(), buffer.size(), "%s", value.c_str());
}

void AppendBinding(std::array<char, 128> &buffer, const std::string &value) {
    std::vector<std::string> existing = SplitKeyList(buffer.data());
    if (std::find(existing.begin(), existing.end(), value) != existing.end()) {
        return;
    }
    existing.push_back(value);
    WriteBuffer(buffer, JoinEntries(existing));
}

std::optional<std::string> DetectKeyboardBinding() {
    const struct { ImGuiKey key; const char *name; } namedKeys[] = {
        {ImGuiKey_Space, "SPACE"},
        {ImGuiKey_Enter, "ENTER"},
        {ImGuiKey_Escape, "ESCAPE"},
        {ImGuiKey_Tab, "TAB"},
        {ImGuiKey_Backspace, "BACKSPACE"},
        {ImGuiKey_LeftArrow, "LEFT"},
        {ImGuiKey_RightArrow, "RIGHT"},
        {ImGuiKey_UpArrow, "UP"},
        {ImGuiKey_DownArrow, "DOWN"},
        {ImGuiKey_LeftBracket, "LEFT_BRACKET"},
        {ImGuiKey_RightBracket, "RIGHT_BRACKET"},
        {ImGuiKey_Minus, "MINUS"},
        {ImGuiKey_Equal, "EQUAL"},
        {ImGuiKey_Apostrophe, "APOSTROPHE"},
        {ImGuiKey_GraveAccent, "GRAVE_ACCENT"},
        {ImGuiKey_Home, "HOME"},
        {ImGuiKey_End, "END"},
        {ImGuiKey_PageUp, "PAGE_UP"},
        {ImGuiKey_PageDown, "PAGE_DOWN"},
        {ImGuiKey_Insert, "INSERT"},
        {ImGuiKey_Delete, "DELETE"},
        {ImGuiKey_CapsLock, "CAPS_LOCK"},
        {ImGuiKey_NumLock, "NUM_LOCK"},
        {ImGuiKey_ScrollLock, "SCROLL_LOCK"},
        {ImGuiKey_LeftShift, "LEFT_SHIFT"},
        {ImGuiKey_RightShift, "RIGHT_SHIFT"},
        {ImGuiKey_LeftCtrl, "LEFT_CONTROL"},
        {ImGuiKey_RightCtrl, "RIGHT_CONTROL"},
        {ImGuiKey_LeftAlt, "LEFT_ALT"},
        {ImGuiKey_RightAlt, "RIGHT_ALT"},
        {ImGuiKey_LeftSuper, "LEFT_SUPER"},
        {ImGuiKey_RightSuper, "RIGHT_SUPER"},
        {ImGuiKey_Menu, "MENU"}
    };

    for (int key = ImGuiKey_A; key <= ImGuiKey_Z; ++key) {
        if (ImGui::IsKeyPressed(static_cast<ImGuiKey>(key))) {
            char name[2] = { static_cast<char>('A' + (key - ImGuiKey_A)), '\0' };
            return std::string(name);
        }
    }

    for (int key = ImGuiKey_0; key <= ImGuiKey_9; ++key) {
        if (ImGui::IsKeyPressed(static_cast<ImGuiKey>(key))) {
            char name[2] = { static_cast<char>('0' + (key - ImGuiKey_0)), '\0' };
            return std::string(name);
        }
    }

    for (int key = ImGuiKey_F1; key <= ImGuiKey_F12; ++key) {
        if (ImGui::IsKeyPressed(static_cast<ImGuiKey>(key))) {
            return "F" + std::to_string(1 + (key - ImGuiKey_F1));
        }
    }

    for (const auto &entry : namedKeys) {
        if (ImGui::IsKeyPressed(entry.key)) {
            return std::string(entry.name);
        }
    }

    return std::nullopt;
}

std::optional<std::string> DetectMouseBinding(bool skipCapture) {
    if (skipCapture) {
        return std::nullopt;
    }

    const struct { int button; const char *name; } mouseButtons[] = {
        {0, "LEFT_MOUSE"},
        {1, "RIGHT_MOUSE"},
        {2, "MIDDLE_MOUSE"},
        {3, "MOUSE4"},
        {4, "MOUSE5"},
        {5, "MOUSE6"},
        {6, "MOUSE7"},
        {7, "MOUSE8"}
    };

    for (const auto &entry : mouseButtons) {
        if (ImGui::IsMouseClicked(entry.button)) {
            return std::string(entry.name);
        }
    }

    return std::nullopt;
}

std::optional<std::string> DetectControllerBinding() {
#ifdef ImGuiKey_GamepadStart
    const struct { ImGuiKey key; const char *name; } gamepadKeys[] = {
        {ImGuiKey_GamepadStart, "GAMEPAD_START"},
        {ImGuiKey_GamepadBack, "GAMEPAD_BACK"},
        {ImGuiKey_GamepadFaceDown, "GAMEPAD_A"},
        {ImGuiKey_GamepadFaceRight, "GAMEPAD_B"},
        {ImGuiKey_GamepadFaceLeft, "GAMEPAD_X"},
        {ImGuiKey_GamepadFaceUp, "GAMEPAD_Y"},
        {ImGuiKey_GamepadDpadLeft, "GAMEPAD_DPAD_LEFT"},
        {ImGuiKey_GamepadDpadRight, "GAMEPAD_DPAD_RIGHT"},
        {ImGuiKey_GamepadDpadUp, "GAMEPAD_DPAD_UP"},
        {ImGuiKey_GamepadDpadDown, "GAMEPAD_DPAD_DOWN"},
        {ImGuiKey_GamepadL1, "GAMEPAD_LB"},
        {ImGuiKey_GamepadR1, "GAMEPAD_RB"},
        {ImGuiKey_GamepadL2, "GAMEPAD_LT"},
        {ImGuiKey_GamepadR2, "GAMEPAD_RT"},
        {ImGuiKey_GamepadL3, "GAMEPAD_LS"},
        {ImGuiKey_GamepadR3, "GAMEPAD_RS"}
    };

    for (const auto &entry : gamepadKeys) {
        if (ImGui::IsKeyPressed(entry.key)) {
            return std::string(entry.name);
        }
    }
#endif

    return std::nullopt;
}

} // namespace

namespace ui {

void ConsoleView::drawSettingsPanel(const MessageColors &colors) {
    static_assert(std::size(kKeybindings) == ConsoleView::kKeybindingCount,
                  "kKeybindings must match buffer sizing.");

    if (!settingsLoaded) {
        settingsLoaded = true;
        settingsStatusText.clear();
        settingsStatusIsError = false;
        selectedBindingIndex = -1;

        bz::json::Value userConfig;
        bool loadedConfig = loadUserConfig(userConfig);
        if (!loadedConfig) {
            settingsStatusText = "Failed to load user config; showing defaults.";
            settingsStatusIsError = true;
        }

        const bz::json::Value *bindingsNode = nullptr;
        if (auto it = userConfig.find("keybindings"); it != userConfig.end() && it->is_object()) {
            bindingsNode = &(*it);
        }
        const bz::json::Value *controllerNode = nullptr;
        if (auto guiIt = userConfig.find("gui"); guiIt != userConfig.end() && guiIt->is_object()) {
            if (auto keyIt = guiIt->find("keybindings"); keyIt != guiIt->end() && keyIt->is_object()) {
                if (auto controllerIt = keyIt->find("controller"); controllerIt != keyIt->end() && controllerIt->is_object()) {
                    controllerNode = &(*controllerIt);
                }
            }
        }

        for (std::size_t i = 0; i < kKeybindingCount; ++i) {
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
                        if (IsMouseBindingName(value)) {
                            mouseEntries.push_back(value);
                        } else {
                            keyboardEntries.push_back(value);
                        }
                    }
                }
            }

            if (keyboardEntries.empty() && mouseEntries.empty()) {
                const std::vector<std::string> defaults = SplitKeyList(kKeybindings[i].defaults);
                for (const auto &value : defaults) {
                    if (IsMouseBindingName(value)) {
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

            WriteBuffer(keybindingKeyboardBuffers[i], JoinEntries(keyboardEntries));
            WriteBuffer(keybindingMouseBuffers[i], JoinEntries(mouseEntries));
            WriteBuffer(keybindingControllerBuffers[i], JoinEntries(controllerEntries));
        }
    }

    ImGui::TextUnformatted("Bindings");
    ImGui::TextDisabled("Select a cell, then press a key/button to add it. Changes apply on next launch.");
    ImGui::Spacing();

    bool selectionChanged = false;
    if (ImGui::BeginTable("KeybindingsTable", 4, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed, 180.0f);
        ImGui::TableSetupColumn("Keyboard");
        ImGui::TableSetupColumn("Mouse");
        ImGui::TableSetupColumn("Controller");
        ImGui::TableHeadersRow();
        for (std::size_t i = 0; i < kKeybindingCount; ++i) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(kKeybindings[i].label);

            auto drawBindingCell = [&](BindingColumn column,
                                       std::array<char, 128> &buffer,
                                       const char *columnId) {
                const bool isSelected = (selectedBindingIndex == static_cast<int>(i) &&
                                         selectedBindingColumn == column);
                std::string display = buffer.data();
                if (display.empty()) {
                    display = "Unbound";
                }
                std::string label = display + "##Bind_" + kKeybindings[i].action + "_" + columnId;
                if (ImGui::Selectable(label.c_str(), isSelected)) {
                    selectedBindingIndex = static_cast<int>(i);
                    selectedBindingColumn = column;
                    selectionChanged = true;
                }
            };

            ImGui::TableSetColumnIndex(1);
            drawBindingCell(BindingColumn::Keyboard, keybindingKeyboardBuffers[i], "Keyboard");
            ImGui::TableSetColumnIndex(2);
            drawBindingCell(BindingColumn::Mouse, keybindingMouseBuffers[i], "Mouse");
            ImGui::TableSetColumnIndex(3);
            drawBindingCell(BindingColumn::Controller, keybindingControllerBuffers[i], "Controller");
        }
        ImGui::EndTable();
    }

    ImGui::Spacing();
    ImGui::TextDisabled("Selected cell: %s / %s",
                        selectedBindingIndex >= 0 ? kKeybindings[selectedBindingIndex].label : "None",
                        selectedBindingIndex >= 0
                            ? (selectedBindingColumn == BindingColumn::Keyboard ? "Keyboard"
                                : (selectedBindingColumn == BindingColumn::Mouse ? "Mouse" : "Controller"))
                            : "None");

    if (selectedBindingIndex >= 0) {
        const bool skipMouseCapture = selectionChanged || ImGui::IsAnyItemActive();
        std::optional<std::string> captured;

        if (selectedBindingColumn == BindingColumn::Keyboard) {
            captured = DetectKeyboardBinding();
            if (captured) {
                AppendBinding(keybindingKeyboardBuffers[selectedBindingIndex], *captured);
            }
        } else if (selectedBindingColumn == BindingColumn::Mouse) {
            captured = DetectMouseBinding(skipMouseCapture);
            if (captured) {
                AppendBinding(keybindingMouseBuffers[selectedBindingIndex], *captured);
            }
        } else {
            captured = DetectControllerBinding();
            if (captured) {
                AppendBinding(keybindingControllerBuffers[selectedBindingIndex], *captured);
            }
        }

    }

    bool saveClicked = false;
    bool resetClicked = false;
    bool clearClicked = false;
    const bool hasButtonFont = (buttonFont != nullptr);
    if (hasButtonFont) {
        ImGui::PushFont(buttonFont);
    }
    ImGui::PushStyleColor(ImGuiCol_Text, buttonColor);
    if (ImGui::Button("Clear Selected")) {
        clearClicked = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Save Bindings")) {
        saveClicked = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset to Defaults")) {
        resetClicked = true;
    }
    ImGui::PopStyleColor();
    if (hasButtonFont) {
        ImGui::PopFont();
    }

    if (clearClicked) {
        if (selectedBindingIndex >= 0) {
            if (selectedBindingColumn == BindingColumn::Keyboard) {
                keybindingKeyboardBuffers[selectedBindingIndex][0] = '\0';
            } else if (selectedBindingColumn == BindingColumn::Mouse) {
                keybindingMouseBuffers[selectedBindingIndex][0] = '\0';
            } else {
                keybindingControllerBuffers[selectedBindingIndex][0] = '\0';
            }
        }
    }

    if (saveClicked) {
        bz::json::Value userConfig;
        if (!loadUserConfig(userConfig)) {
            settingsStatusText = "Failed to load user config.";
            settingsStatusIsError = true;
        } else {
            bz::json::Value keybindings = bz::json::Object();
            bz::json::Value controllerBindings = bz::json::Object();
            bool hasBindings = false;
            bool hasControllerBindings = false;
            for (std::size_t i = 0; i < kKeybindingCount; ++i) {
                std::vector<std::string> keyboardValues = SplitKeyList(keybindingKeyboardBuffers[i].data());
                std::vector<std::string> mouseValues = SplitKeyList(keybindingMouseBuffers[i].data());
                std::vector<std::string> controllerValues = SplitKeyList(keybindingControllerBuffers[i].data());

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
                    controllerBindings[kKeybindings[i].action] = controllerValues;
                    hasControllerBindings = true;
                }
            }

            if (hasBindings) {
                setNestedConfig(userConfig, {"keybindings"}, keybindings);
            } else {
                eraseNestedConfig(userConfig, {"keybindings"});
            }

            if (hasControllerBindings) {
                setNestedConfig(userConfig, {"gui", "keybindings", "controller"}, controllerBindings);
            } else {
                eraseNestedConfig(userConfig, {"gui", "keybindings", "controller"});
            }

            std::string error;
            if (!saveUserConfig(userConfig, error)) {
                settingsStatusText = error.empty() ? "Failed to save bindings." : error;
                settingsStatusIsError = true;
            } else {
                requestKeybindingsReload();
                settingsStatusText = "Bindings saved.";
                settingsStatusIsError = false;
            }
        }
    }

    if (resetClicked) {
        for (std::size_t i = 0; i < kKeybindingCount; ++i) {
            std::vector<std::string> keyboardEntries;
            std::vector<std::string> mouseEntries;
            const std::vector<std::string> defaults = SplitKeyList(kKeybindings[i].defaults);
            for (const auto &value : defaults) {
                if (IsMouseBindingName(value)) {
                    mouseEntries.push_back(value);
                } else {
                    keyboardEntries.push_back(value);
                }
            }
            WriteBuffer(keybindingKeyboardBuffers[i], JoinEntries(keyboardEntries));
            WriteBuffer(keybindingMouseBuffers[i], JoinEntries(mouseEntries));
            keybindingControllerBuffers[i][0] = '\0';
        }

        bz::json::Value userConfig;
        if (!loadUserConfig(userConfig)) {
            settingsStatusText = "Failed to load user config.";
            settingsStatusIsError = true;
        } else {
            eraseNestedConfig(userConfig, {"keybindings"});
            eraseNestedConfig(userConfig, {"gui", "keybindings", "controller"});
            std::string error;
            if (!saveUserConfig(userConfig, error)) {
                settingsStatusText = error.empty() ? "Failed to reset bindings." : error;
                settingsStatusIsError = true;
            } else {
                requestKeybindingsReload();
                settingsStatusText = "Bindings reset to defaults.";
                settingsStatusIsError = false;
            }
        }
    }

    if (!settingsStatusText.empty()) {
        ImVec4 statusColor = settingsStatusIsError ? colors.error : colors.notice;
        ImGui::TextColored(statusColor, "%s", settingsStatusText.c_str());
    }

}

} // namespace ui
