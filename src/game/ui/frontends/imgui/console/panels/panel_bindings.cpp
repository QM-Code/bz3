#include "ui/frontends/imgui/console/console.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "common/config_store.hpp"
#include "common/json.hpp"
#include "game/input/bindings.hpp"
#include "ui/console/keybindings.hpp"
#include "ui/ui_config.hpp"

namespace {

const std::vector<std::string> &defaultBindingsForAction(std::string_view action) {
    static const game_input::DefaultBindingsMap kDefaults = game_input::DefaultKeybindings();
    static const std::vector<std::string> kEmpty;
    auto it = kDefaults.find(std::string(action));
    if (it == kDefaults.end()) {
        return kEmpty;
    }
    return it->second;
}

void WriteBuffer(std::array<char, 128> &buffer, const std::string &value) {
    std::snprintf(buffer.data(), buffer.size(), "%s", value.c_str());
}

void AppendBinding(std::array<char, 128> &buffer, const std::string &value) {
    std::vector<std::string> existing = ui::bindings::SplitBindings(buffer.data());
    if (std::find(existing.begin(), existing.end(), value) == existing.end()) {
        existing.push_back(value);
        WriteBuffer(buffer, ui::bindings::JoinBindings(existing));
    }
}

std::optional<std::string> DetectKeyboardBinding() {
    for (int key = ImGuiKey_NamedKey_BEGIN; key < ImGuiKey_NamedKey_END; ++key) {
        const ImGuiKey keyValue = static_cast<ImGuiKey>(key);
#ifdef ImGuiKey_GamepadStart
        if (keyValue >= ImGuiKey_GamepadStart && keyValue <= ImGuiKey_GamepadR3) {
            continue;
        }
#endif
        if (ImGui::IsKeyPressed(keyValue, false)) {
            const char *name = ImGui::GetKeyName(keyValue);
            if (name && *name) {
                return std::string(name);
            }
        }
    }
    return std::nullopt;
}

std::optional<std::string> DetectMouseBinding(bool skipCapture) {
    if (skipCapture) {
        return std::nullopt;
    }
    const std::array<std::pair<int, const char *>, 8> mouseButtons = {{
        {ImGuiMouseButton_Left, "LEFT_MOUSE"},
        {ImGuiMouseButton_Right, "RIGHT_MOUSE"},
        {ImGuiMouseButton_Middle, "MIDDLE_MOUSE"},
        {3, "MOUSE4"},
        {4, "MOUSE5"},
        {5, "MOUSE6"},
        {6, "MOUSE7"},
        {7, "MOUSE8"}
    }};
    for (const auto &entry : mouseButtons) {
        if (ImGui::IsMouseClicked(entry.first)) {
            return std::string(entry.second);
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

void ConsoleView::drawBindingsPanel(const MessageColors &colors) {
    const auto defs = ui::bindings::Definitions();
    auto resetBindings = [&]() {
        const std::size_t count = std::min(defs.size(), kKeybindingCount);
        for (std::size_t i = 0; i < count; ++i) {
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
            WriteBuffer(keybindingKeyboardBuffers[i], ui::bindings::JoinBindings(keyboardEntries));
            WriteBuffer(keybindingMouseBuffers[i], ui::bindings::JoinBindings(mouseEntries));
            keybindingControllerBuffers[i][0] = '\0';
        }

        ui::UiConfig::EraseKeybindings();
        ui::UiConfig::EraseControllerKeybindings();
        requestKeybindingsReload();
        bindingsStatusText = "Bindings reset to defaults.";
        bindingsStatusIsError = false;
    };
    auto saveBindings = [&]() -> bool {
        bz::json::Value keybindings = bz::json::Object();
        bz::json::Value controllerBindings = bz::json::Object();
        bool hasBindings = false;
        bool hasControllerBindings = false;
        const std::size_t count = std::min(defs.size(), kKeybindingCount);
        for (std::size_t i = 0; i < count; ++i) {
            std::vector<std::string> keyboardValues = ui::bindings::SplitBindings(keybindingKeyboardBuffers[i].data());
            std::vector<std::string> mouseValues = ui::bindings::SplitBindings(keybindingMouseBuffers[i].data());
            std::vector<std::string> controllerValues = ui::bindings::SplitBindings(keybindingControllerBuffers[i].data());

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
                controllerBindings[defs[i].action] = controllerValues;
                hasControllerBindings = true;
            }
        }

        bool ok = bz::config::ConfigStore::Initialized();
        if (hasBindings) {
            ok = ok && ui::UiConfig::SetKeybindings(keybindings);
        } else {
            ok = ok && ui::UiConfig::EraseKeybindings();
        }
        if (hasControllerBindings) {
            ok = ok && ui::UiConfig::SetControllerKeybindings(controllerBindings);
        } else {
            ok = ok && ui::UiConfig::EraseControllerKeybindings();
        }

        if (!ok) {
            bindingsStatusText = "Failed to save bindings.";
            bindingsStatusIsError = true;
            return false;
        }
        requestKeybindingsReload();
        bindingsStatusText = "Bindings saved.";
        bindingsStatusIsError = false;
        return true;
    };

    if (!bindingsLoaded) {
        bindingsLoaded = true;
        bindingsStatusText.clear();
        bindingsStatusIsError = false;
        selectedBindingIndex = -1;

        if (!bz::config::ConfigStore::Initialized()) {
            bindingsStatusText = "Failed to load config; showing defaults.";
            bindingsStatusIsError = true;
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

        const std::size_t count = std::min(defs.size(), kKeybindingCount);
        for (std::size_t i = 0; i < count; ++i) {
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

            WriteBuffer(keybindingKeyboardBuffers[i], ui::bindings::JoinBindings(keyboardEntries));
            WriteBuffer(keybindingMouseBuffers[i], ui::bindings::JoinBindings(mouseEntries));
            WriteBuffer(keybindingControllerBuffers[i], ui::bindings::JoinBindings(controllerEntries));
        }
    }

    ImGui::TextDisabled("Select a cell, then press a key/button to add it. Changes apply on next launch.");
    ImGui::Spacing();

    const int previousBindingIndex = selectedBindingIndex;
    const BindingColumn previousBindingColumn = selectedBindingColumn;
    bool selectionChanged = false;
    bool selectedCellHovered = false;
    bool anyBindingCellHovered = false;
    if (ImGui::BeginTable("KeybindingsTable", 4, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed, 180.0f);
        ImGui::TableSetupColumn("Keyboard");
        ImGui::TableSetupColumn("Mouse");
        ImGui::TableSetupColumn("Controller");
        ImGui::TableHeadersRow();
        const std::size_t count = std::min(defs.size(), kKeybindingCount);
        for (std::size_t i = 0; i < count; ++i) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(defs[i].label);

            auto drawBindingCell = [&](BindingColumn column,
                                       std::array<char, 128> &buffer,
                                       const char *columnId) {
                const bool isSelected = (selectedBindingIndex == static_cast<int>(i) &&
                                         selectedBindingColumn == column);
                std::string display = buffer.data();
                if (display.empty()) {
                    display = "Unbound";
                }
                std::string label = display + "##Bind_" + defs[i].action + "_" + columnId;
                if (ImGui::Selectable(label.c_str(), isSelected)) {
                    selectedBindingIndex = static_cast<int>(i);
                    selectedBindingColumn = column;
                    selectionChanged = true;
                }
                if (isSelected && ImGui::IsItemHovered()) {
                    selectedCellHovered = true;
                }
                if (ImGui::IsItemHovered()) {
                    anyBindingCellHovered = true;
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

    if (previousBindingIndex >= 0 && previousBindingColumn == BindingColumn::Keyboard) {
        if (selectionChanged) {
            saveBindings();
        } else if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !selectedCellHovered) {
            if (anyBindingCellHovered || !ImGui::IsAnyItemHovered()) {
                saveBindings();
                if (!anyBindingCellHovered) {
                    selectedBindingIndex = -1;
                }
            }
        }
    }

    ImGui::Spacing();
    const char *selectedLabel = "None";
    const char *selectedColumn = "None";
    if (selectedBindingIndex >= 0 && selectedBindingIndex < static_cast<int>(defs.size())) {
        selectedLabel = defs[static_cast<std::size_t>(selectedBindingIndex)].label;
        selectedColumn = (selectedBindingColumn == BindingColumn::Keyboard)
            ? "Keyboard"
            : (selectedBindingColumn == BindingColumn::Mouse ? "Mouse" : "Controller");
    }
    ImGui::TextDisabled("Selected cell: %s / %s", selectedLabel, selectedColumn);

    if (selectedBindingIndex >= 0) {
        const bool skipMouseCapture = selectionChanged || ImGui::IsAnyItemActive();
        std::optional<std::string> captured;

        if (selectedBindingColumn == BindingColumn::Keyboard) {
            captured = DetectKeyboardBinding();
            if (captured) {
                AppendBinding(keybindingKeyboardBuffers[selectedBindingIndex], *captured);
            }
        } else if (selectedBindingColumn == BindingColumn::Mouse) {
            if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                saveBindings();
                selectedBindingIndex = -1;
            } else {
                captured = DetectMouseBinding(skipMouseCapture);
                if (captured) {
                    AppendBinding(keybindingMouseBuffers[selectedBindingIndex], *captured);
                }
            }
        } else {
            captured = DetectControllerBinding();
            if (captured) {
                AppendBinding(keybindingControllerBuffers[selectedBindingIndex], *captured);
            }
        }
    }

    ImGui::Spacing();
    bool saveClicked = ImGui::Button("Save Bindings");
    ImGui::SameLine();
    bool resetClicked = ImGui::Button("Reset to Defaults");
    ImGui::SameLine();
    if (ImGui::Button("Clear Selected")) {
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
        saveBindings();
    }

    if (resetClicked) {
        ImGui::OpenPopup("Reset Bindings?");
        bindingsResetConfirmOpen = true;
    }

    if (bindingsResetConfirmOpen && ImGui::BeginPopupModal("Reset Bindings?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextWrapped("Reset all keybindings to defaults? This will overwrite your custom bindings.");
        ImGui::Spacing();
        if (ImGui::Button("Reset")) {
            resetBindings();
            bindingsResetConfirmOpen = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            bindingsResetConfirmOpen = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (!bindingsStatusText.empty()) {
        ImVec4 statusColor = bindingsStatusIsError ? colors.error : colors.notice;
        ImGui::TextColored(statusColor, "%s", bindingsStatusText.c_str());
    }
}

} // namespace ui
