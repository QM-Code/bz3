#include "input/input.hpp"

#include "platform/window.hpp"
#include "common/data_path_resolver.hpp"

#include <algorithm>
#include <cctype>
#include <nlohmann/json.hpp>
#include <optional>
#include <spdlog/spdlog.h>
#include <string>
#include <unordered_map>
#include <sstream>

namespace {

using Binding = Input::Binding;

std::string NormalizeKeyName(std::string name) {
    std::transform(name.begin(), name.end(), name.begin(), [](unsigned char ch) {
        if (ch == ' ' || ch == '-') {
            return '_';
        }
        return static_cast<char>(std::toupper(ch));
    });
    return name;
}

std::optional<Binding> BindingFromName(std::string name) {
    name = NormalizeKeyName(std::move(name));

    if (name.size() == 1) {
        const char ch = name[0];
        if (ch >= 'A' && ch <= 'Z') {
            return Binding{Binding::Type::Key, static_cast<platform::Key>(static_cast<int>(platform::Key::A) + (ch - 'A')), platform::MouseButton::Left};
        }
        if (ch >= '0' && ch <= '9') {
            return Binding{Binding::Type::Key, static_cast<platform::Key>(static_cast<int>(platform::Key::Num0) + (ch - '0')), platform::MouseButton::Left};
        }
        if (ch == ']') {
            return Binding{Binding::Type::Key, platform::Key::RightBracket, platform::MouseButton::Left};
        }
        if (ch == '[') {
            return Binding{Binding::Type::Key, platform::Key::LeftBracket, platform::MouseButton::Left};
        }
    }

    if (name.size() > 1 && name[0] == 'F') {
        try {
            const int fnNumber = std::stoi(name.substr(1));
            if (fnNumber >= 1 && fnNumber <= 25) {
                return Binding{Binding::Type::Key, static_cast<platform::Key>(static_cast<int>(platform::Key::F1) + (fnNumber - 1)), platform::MouseButton::Left};
            }
        } catch (...) {
            return std::nullopt;
        }
    }

    auto parseMouseNumbered = [](const std::string &suffix) -> std::optional<platform::MouseButton> {
        if (suffix.empty()) {
            return std::nullopt;
        }
        try {
            const int idx = std::stoi(suffix);
            if (idx >= 1 && idx <= 8) {
                return static_cast<platform::MouseButton>(static_cast<int>(platform::MouseButton::Left) + (idx - 1));
            }
        } catch (...) {
            return std::nullopt;
        }
        return std::nullopt;
    };

    auto stripLeadingUnderscores = [](const std::string &text) {
        const auto pos = text.find_first_not_of('_');
        if (pos == std::string::npos) {
            return std::string{};
        }
        return text.substr(pos);
    };

    if (name.rfind("MOUSE", 0) == 0) {
        const auto suffix = name.substr(5); // after "MOUSE"
        if (suffix == "_LEFT" || suffix == "LEFT" || suffix == "_1" || suffix == "1") {
            return Binding{Binding::Type::MouseButton, platform::Key::Unknown, platform::MouseButton::Left};
        }
        if (suffix == "_RIGHT" || suffix == "RIGHT" || suffix == "_2" || suffix == "2") {
            return Binding{Binding::Type::MouseButton, platform::Key::Unknown, platform::MouseButton::Right};
        }
        if (suffix == "_MIDDLE" || suffix == "MIDDLE" || suffix == "_3" || suffix == "3") {
            return Binding{Binding::Type::MouseButton, platform::Key::Unknown, platform::MouseButton::Middle};
        }
        const auto numericSuffix = stripLeadingUnderscores(suffix);
        if (auto button = parseMouseNumbered(numericSuffix)) {
            return Binding{Binding::Type::MouseButton, platform::Key::Unknown, *button};
        }
    }

    if (name == "LEFT_MOUSE") {
        return Binding{Binding::Type::MouseButton, platform::Key::Unknown, platform::MouseButton::Left};
    }
    if (name == "RIGHT_MOUSE") {
        return Binding{Binding::Type::MouseButton, platform::Key::Unknown, platform::MouseButton::Right};
    }
    if (name == "MIDDLE_MOUSE") {
        return Binding{Binding::Type::MouseButton, platform::Key::Unknown, platform::MouseButton::Middle};
    }

    if (name.rfind("MOUSE_BUTTON_", 0) == 0) {
        if (auto button = parseMouseNumbered(name.substr(std::string("MOUSE_BUTTON_").size()))) {
            return Binding{Binding::Type::MouseButton, platform::Key::Unknown, *button};
        }
    }

    static const std::unordered_map<std::string, platform::Key> kNamedKeys = {
        {"SPACE", platform::Key::Space},
        {"ESCAPE", platform::Key::Escape},
        {"ENTER", platform::Key::Enter},
        {"RETURN", platform::Key::Enter},
        {"TAB", platform::Key::Tab},
        {"BACKSPACE", platform::Key::Backspace},
        {"LEFT", platform::Key::Left},
        {"RIGHT", platform::Key::Right},
        {"UP", platform::Key::Up},
        {"DOWN", platform::Key::Down},
        {"LEFT_BRACKET", platform::Key::LeftBracket},
        {"RIGHT_BRACKET", platform::Key::RightBracket},
        {"MINUS", platform::Key::Minus},
        {"EQUAL", platform::Key::Equal},
        {"APOSTROPHE", platform::Key::Apostrophe},
        {"GRAVE_ACCENT", platform::Key::GraveAccent},
        {"WORLD_1", platform::Key::World1},
        {"WORLD_2", platform::Key::World2},
        {"LEFT_SHIFT", platform::Key::LeftShift},
        {"RIGHT_SHIFT", platform::Key::RightShift},
        {"LEFT_CONTROL", platform::Key::LeftControl},
        {"RIGHT_CONTROL", platform::Key::RightControl},
        {"LEFT_ALT", platform::Key::LeftAlt},
        {"RIGHT_ALT", platform::Key::RightAlt},
        {"LEFT_SUPER", platform::Key::LeftSuper},
        {"RIGHT_SUPER", platform::Key::RightSuper},
        {"MENU", platform::Key::Menu},
        {"HOME", platform::Key::Home},
        {"END", platform::Key::End},
        {"PAGE_UP", platform::Key::PageUp},
        {"PAGE_DOWN", platform::Key::PageDown},
        {"INSERT", platform::Key::Insert},
        {"DELETE", platform::Key::Delete},
        {"CAPS_LOCK", platform::Key::CapsLock},
        {"NUM_LOCK", platform::Key::NumLock},
        {"SCROLL_LOCK", platform::Key::ScrollLock}
    };

    if (const auto it = kNamedKeys.find(name); it != kNamedKeys.end()) {
        return Binding{Binding::Type::Key, it->second, platform::MouseButton::Left};
    }

    return std::nullopt;
}

std::vector<Binding> ParseKeyBinding(const nlohmann::json *keybindings,
                                     const char *action,
                                     std::initializer_list<const char*> defaults) {
    std::vector<Binding> bindings;

    auto addKey = [&](const std::string &name) {
        if (auto binding = BindingFromName(name)) {
            if (std::none_of(bindings.begin(), bindings.end(), [&](const Binding &b) { return b.type == binding->type && b.key == binding->key && b.mouseButton == binding->mouseButton; })) {
                bindings.push_back(*binding);
            }
        } else {
            spdlog::warn("Input: Unknown key '{}' for action '{}'", name, action);
        }
    };

    if (keybindings) {
        const auto it = keybindings->find(action);
        if (it != keybindings->end()) {
            if (!it->is_array()) {
                spdlog::warn("Input: keybindings.{} must be an array of strings", action);
            } else {
                for (const auto &value : *it) {
                    if (value.is_string()) {
                        addKey(value.get<std::string>());
                    } else {
                        spdlog::warn("Input: keybindings.{} entries must be strings", action);
                    }
                }
            }
        }
    }

    if (bindings.empty()) {
        for (const auto *name : defaults) {
            addKey(name);
        }
    }

    return bindings;
}

std::string BindingToString(const Binding &b) {
    if (b.type == Binding::Type::MouseButton) {
        switch (b.mouseButton) {
            case platform::MouseButton::Left: return "Left Mouse";
            case platform::MouseButton::Right: return "Right Mouse";
            case platform::MouseButton::Middle: return "Middle Mouse";
            case platform::MouseButton::Button4: return "Mouse 4";
            case platform::MouseButton::Button5: return "Mouse 5";
            case platform::MouseButton::Button6: return "Mouse 6";
            case platform::MouseButton::Button7: return "Mouse 7";
            case platform::MouseButton::Button8: return "Mouse 8";
        }
    }

    if (b.key >= platform::Key::F1 && b.key <= platform::Key::F25) {
        return "F" + std::to_string(1 + static_cast<int>(b.key) - static_cast<int>(platform::Key::F1));
    }

    if (b.key >= platform::Key::A && b.key <= platform::Key::Z) {
        char c = static_cast<char>('A' + (static_cast<int>(b.key) - static_cast<int>(platform::Key::A)));
        return std::string(1, c);
    }

    if (b.key >= platform::Key::Num0 && b.key <= platform::Key::Num9) {
        char c = static_cast<char>('0' + (static_cast<int>(b.key) - static_cast<int>(platform::Key::Num0)));
        return std::string(1, c);
    }

    static const std::unordered_map<platform::Key, std::string> kKeyNames = {
        {platform::Key::Space, "Space"},
        {platform::Key::Escape, "Escape"},
        {platform::Key::Enter, "Enter"},
        {platform::Key::Tab, "Tab"},
        {platform::Key::Backspace, "Backspace"},
        {platform::Key::Left, "Left"},
        {platform::Key::Right, "Right"},
        {platform::Key::Up, "Up"},
        {platform::Key::Down, "Down"},
        {platform::Key::LeftBracket, "["},
        {platform::Key::RightBracket, "]"},
        {platform::Key::Minus, "-"},
        {platform::Key::Equal, "="},
        {platform::Key::Apostrophe, "'"},
        {platform::Key::GraveAccent, "`"},
        {platform::Key::LeftShift, "Left Shift"},
        {platform::Key::RightShift, "Right Shift"},
        {platform::Key::LeftControl, "Left Ctrl"},
        {platform::Key::RightControl, "Right Ctrl"},
        {platform::Key::LeftAlt, "Left Alt"},
        {platform::Key::RightAlt, "Right Alt"},
        {platform::Key::LeftSuper, "Left Super"},
        {platform::Key::RightSuper, "Right Super"},
        {platform::Key::Menu, "Menu"},
        {platform::Key::Home, "Home"},
        {platform::Key::End, "End"},
        {platform::Key::PageUp, "Page Up"},
        {platform::Key::PageDown, "Page Down"},
        {platform::Key::Insert, "Insert"},
        {platform::Key::Delete, "Delete"},
        {platform::Key::CapsLock, "Caps Lock"},
        {platform::Key::NumLock, "Num Lock"},
        {platform::Key::ScrollLock, "Scroll Lock"}
    };

    if (const auto it = kKeyNames.find(b.key); it != kKeyNames.end()) {
        return it->second;
    }

    return "Key";
}

std::string JoinBindingStrings(const std::vector<Binding> &bindings) {
    std::ostringstream oss;
    for (std::size_t i = 0; i < bindings.size(); ++i) {
        if (i > 0) {
            oss << " or ";
        }
        oss << BindingToString(bindings[i]);
    }
    return oss.str();
}

} // namespace

Input::Input(platform::Window &window) {
    this->window = &window;
    this->inputState = {};
    loadKeyBindings();
}

void Input::loadKeyBindings() {
    std::optional<nlohmann::json> keybindingsConfig;

    if (auto keybindingsOpt = bz::data::ConfigValueCopy("keybindings")) {
        if (keybindingsOpt->is_object()) {
            keybindingsConfig = std::move(keybindingsOpt);
        } else {
            spdlog::warn("Input: 'keybindings' exists but is not a JSON object; falling back to defaults");
        }
    }

    const nlohmann::json *keybindingsJson = keybindingsConfig ? &(*keybindingsConfig) : nullptr;

    keyBindings.fire = ParseKeyBinding(keybindingsJson, "fire", {"F", "E", "LEFT_MOUSE"});
    keyBindings.spawn = ParseKeyBinding(keybindingsJson, "spawn", {"U"});
    keyBindings.jump = ParseKeyBinding(keybindingsJson, "jump", {"SPACE"});
    keyBindings.quickQuit = ParseKeyBinding(keybindingsJson, "quickQuit", {"F12"});
    keyBindings.chat = ParseKeyBinding(keybindingsJson, "chat", {"T"});
    keyBindings.escape = ParseKeyBinding(keybindingsJson, "escape", {"ESCAPE"});
    keyBindings.toggleFullscreen = ParseKeyBinding(keybindingsJson, "toggleFullscreen", {"RIGHT_BRACKET"});
    keyBindings.moveLeft = ParseKeyBinding(keybindingsJson, "moveLeft", {"LEFT", "J"});
    keyBindings.moveRight = ParseKeyBinding(keybindingsJson, "moveRight", {"RIGHT", "L"});
    keyBindings.moveForward = ParseKeyBinding(keybindingsJson, "moveForward", {"UP", "I"});
    keyBindings.moveBackward = ParseKeyBinding(keybindingsJson, "moveBackward", {"DOWN", "K"});
}

bool Input::keyMatches(const std::vector<Binding> &binding, platform::Key key) const {
    return std::any_of(binding.begin(), binding.end(), [&](const Binding &b) {
        return b.type == Binding::Type::Key && b.key == key;
    });
}

bool Input::mouseMatches(const std::vector<Binding> &binding, platform::MouseButton button) const {
    return std::any_of(binding.begin(), binding.end(), [&](const Binding &b) {
        return b.type == Binding::Type::MouseButton && b.mouseButton == button;
    });
}

bool Input::isBindingPressed(const std::vector<Binding> &binding) const {
    return std::any_of(binding.begin(), binding.end(), [this](const Binding &b) {
        if (!window) {
            return false;
        }
        if (b.type == Binding::Type::Key) {
            return window->isKeyDown(b.key);
        }
        return window->isMouseDown(b.mouseButton);
    });
}

void Input::update(const std::vector<platform::Event> &events) {
    inputState = { 0 };

    for (const auto &event : events) {
        if (event.type == platform::EventType::KeyDown) {
            if (keyMatches(keyBindings.fire, event.key)) {
                inputState.fire = true;
            }
            if (keyMatches(keyBindings.spawn, event.key)) {
                inputState.spawn = true;
            }
            if (keyMatches(keyBindings.quickQuit, event.key)) {
                inputState.quickQuit = true;
            }
            if (keyMatches(keyBindings.toggleFullscreen, event.key)) {
                inputState.toggleFullscreen = true;
            }
            if (keyMatches(keyBindings.chat, event.key)) {
                inputState.chat = true;
            }
            if (keyMatches(keyBindings.escape, event.key)) {
                inputState.escape = true;
            }
        } else if (event.type == platform::EventType::MouseButtonDown) {
            if (mouseMatches(keyBindings.fire, event.mouseButton)) {
                inputState.fire = true;
            }
            if (mouseMatches(keyBindings.spawn, event.mouseButton)) {
                inputState.spawn = true;
            }
            if (mouseMatches(keyBindings.quickQuit, event.mouseButton)) {
                inputState.quickQuit = true;
            }
            if (mouseMatches(keyBindings.toggleFullscreen, event.mouseButton)) {
                inputState.toggleFullscreen = true;
            }
            if (mouseMatches(keyBindings.chat, event.mouseButton)) {
                inputState.chat = true;
            }
            if (mouseMatches(keyBindings.escape, event.mouseButton)) {
                inputState.escape = true;
            }
        }
    }

    if (isBindingPressed(keyBindings.moveLeft))
        inputState.movement.x -= 1;
    if (isBindingPressed(keyBindings.moveRight))
        inputState.movement.x += 1;

    if (isBindingPressed(keyBindings.moveForward))
        inputState.movement.y += 1;
    if (isBindingPressed(keyBindings.moveBackward))
        inputState.movement.y -= 1;

    if (isBindingPressed(keyBindings.jump)) {
        inputState.jump = true;
    }
}

const InputState &Input::getInputState() const {
    return inputState;
}

void Input::clearState() {
    inputState = { 0 };
}

std::string Input::bindingListDisplay(const std::vector<Binding> &bindings) const {
    if (bindings.empty()) {
        return "U";
    }
    return JoinBindingStrings(bindings);
}

std::string Input::spawnHintText() const {
    const auto hint = bindingListDisplay(keyBindings.spawn);
    return "Press " + hint + " to spawn";
}
