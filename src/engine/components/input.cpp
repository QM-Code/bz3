#include "engine/components/input.hpp"
#include "engine/types.hpp"
#include "engine/user_pointer.hpp"
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
            return Binding{Binding::Type::Key, GLFW_KEY_A + (ch - 'A')};
        }
        if (ch >= '0' && ch <= '9') {
            return Binding{Binding::Type::Key, GLFW_KEY_0 + (ch - '0')};
        }
        if (ch == ']') {
            return Binding{Binding::Type::Key, GLFW_KEY_RIGHT_BRACKET};
        }
        if (ch == '[') {
            return Binding{Binding::Type::Key, GLFW_KEY_LEFT_BRACKET};
        }
    }

    if (name.size() > 1 && name[0] == 'F') {
        try {
            const int fnNumber = std::stoi(name.substr(1));
            if (fnNumber >= 1 && fnNumber <= 25) {
                return Binding{Binding::Type::Key, GLFW_KEY_F1 + (fnNumber - 1)};
            }
        } catch (...) {
            return std::nullopt;
        }
    }

    auto parseMouseNumbered = [](const std::string &suffix) -> std::optional<int> {
        if (suffix.empty()) {
            return std::nullopt;
        }
        try {
            const int idx = std::stoi(suffix);
            if (idx >= 1 && idx <= 8) {
                return GLFW_MOUSE_BUTTON_1 + (idx - 1);
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
            return Binding{Binding::Type::MouseButton, GLFW_MOUSE_BUTTON_LEFT};
        }
        if (suffix == "_RIGHT" || suffix == "RIGHT" || suffix == "_2" || suffix == "2") {
            return Binding{Binding::Type::MouseButton, GLFW_MOUSE_BUTTON_RIGHT};
        }
        if (suffix == "_MIDDLE" || suffix == "MIDDLE" || suffix == "_3" || suffix == "3") {
            return Binding{Binding::Type::MouseButton, GLFW_MOUSE_BUTTON_MIDDLE};
        }
        const auto numericSuffix = stripLeadingUnderscores(suffix);
        if (auto button = parseMouseNumbered(numericSuffix)) {
            return Binding{Binding::Type::MouseButton, *button};
        }
    }

    if (name == "LEFT_MOUSE") {
        return Binding{Binding::Type::MouseButton, GLFW_MOUSE_BUTTON_LEFT};
    }
    if (name == "RIGHT_MOUSE") {
        return Binding{Binding::Type::MouseButton, GLFW_MOUSE_BUTTON_RIGHT};
    }
    if (name == "MIDDLE_MOUSE") {
        return Binding{Binding::Type::MouseButton, GLFW_MOUSE_BUTTON_MIDDLE};
    }

    if (name.rfind("MOUSE_BUTTON_", 0) == 0) {
        if (auto button = parseMouseNumbered(name.substr(std::string("MOUSE_BUTTON_").size()))) {
            return Binding{Binding::Type::MouseButton, *button};
        }
    }

    static const std::unordered_map<std::string, int> kNamedKeys = {
        {"SPACE", GLFW_KEY_SPACE},
        {"ESCAPE", GLFW_KEY_ESCAPE},
        {"ENTER", GLFW_KEY_ENTER},
        {"RETURN", GLFW_KEY_ENTER},
        {"TAB", GLFW_KEY_TAB},
        {"BACKSPACE", GLFW_KEY_BACKSPACE},
        {"LEFT", GLFW_KEY_LEFT},
        {"RIGHT", GLFW_KEY_RIGHT},
        {"UP", GLFW_KEY_UP},
        {"DOWN", GLFW_KEY_DOWN},
        {"LEFT_BRACKET", GLFW_KEY_LEFT_BRACKET},
        {"RIGHT_BRACKET", GLFW_KEY_RIGHT_BRACKET},
        {"MINUS", GLFW_KEY_MINUS},
        {"EQUAL", GLFW_KEY_EQUAL},
        {"APOSTROPHE", GLFW_KEY_APOSTROPHE},
        {"GRAVE_ACCENT", GLFW_KEY_GRAVE_ACCENT},
        {"WORLD_1", GLFW_KEY_WORLD_1},
        {"WORLD_2", GLFW_KEY_WORLD_2},
        {"LEFT_SHIFT", GLFW_KEY_LEFT_SHIFT},
        {"RIGHT_SHIFT", GLFW_KEY_RIGHT_SHIFT},
        {"LEFT_CONTROL", GLFW_KEY_LEFT_CONTROL},
        {"RIGHT_CONTROL", GLFW_KEY_RIGHT_CONTROL},
        {"LEFT_ALT", GLFW_KEY_LEFT_ALT},
        {"RIGHT_ALT", GLFW_KEY_RIGHT_ALT},
        {"LEFT_SUPER", GLFW_KEY_LEFT_SUPER},
        {"RIGHT_SUPER", GLFW_KEY_RIGHT_SUPER},
        {"MENU", GLFW_KEY_MENU},
        {"HOME", GLFW_KEY_HOME},
        {"END", GLFW_KEY_END},
        {"PAGE_UP", GLFW_KEY_PAGE_UP},
        {"PAGE_DOWN", GLFW_KEY_PAGE_DOWN},
        {"INSERT", GLFW_KEY_INSERT},
        {"DELETE", GLFW_KEY_DELETE},
        {"CAPS_LOCK", GLFW_KEY_CAPS_LOCK},
        {"NUM_LOCK", GLFW_KEY_NUM_LOCK},
        {"SCROLL_LOCK", GLFW_KEY_SCROLL_LOCK}
    };

    if (const auto it = kNamedKeys.find(name); it != kNamedKeys.end()) {
        return Binding{Binding::Type::Key, it->second};
    }

    return std::nullopt;
}

std::vector<Binding> ParseKeyBinding(const nlohmann::json *keybindings,
                                     const char *action,
                                     std::initializer_list<const char*> defaults) {
    std::vector<Binding> bindings;

    auto addKey = [&](const std::string &name) {
        if (auto binding = BindingFromName(name)) {
            if (std::none_of(bindings.begin(), bindings.end(), [&](const Binding &b) { return b.type == binding->type && b.code == binding->code; })) {
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
        switch (b.code) {
            case GLFW_MOUSE_BUTTON_LEFT: return "Left Mouse";
            case GLFW_MOUSE_BUTTON_RIGHT: return "Right Mouse";
            case GLFW_MOUSE_BUTTON_MIDDLE: return "Middle Mouse";
            default: return "Mouse " + std::to_string(b.code + 1);
        }
    }

    if (b.code >= GLFW_KEY_F1 && b.code <= GLFW_KEY_F25) {
        return "F" + std::to_string(1 + b.code - GLFW_KEY_F1);
    }

    if (b.code >= GLFW_KEY_A && b.code <= GLFW_KEY_Z) {
        char c = static_cast<char>('A' + (b.code - GLFW_KEY_A));
        return std::string(1, c);
    }

    if (b.code >= GLFW_KEY_0 && b.code <= GLFW_KEY_9) {
        char c = static_cast<char>('0' + (b.code - GLFW_KEY_0));
        return std::string(1, c);
    }

    static const std::unordered_map<int, std::string> kKeyNames = {
        {GLFW_KEY_SPACE, "Space"},
        {GLFW_KEY_ESCAPE, "Escape"},
        {GLFW_KEY_ENTER, "Enter"},
        {GLFW_KEY_TAB, "Tab"},
        {GLFW_KEY_BACKSPACE, "Backspace"},
        {GLFW_KEY_LEFT, "Left"},
        {GLFW_KEY_RIGHT, "Right"},
        {GLFW_KEY_UP, "Up"},
        {GLFW_KEY_DOWN, "Down"},
        {GLFW_KEY_LEFT_BRACKET, "["},
        {GLFW_KEY_RIGHT_BRACKET, "]"},
        {GLFW_KEY_MINUS, "-"},
        {GLFW_KEY_EQUAL, "="},
        {GLFW_KEY_APOSTROPHE, "'"},
        {GLFW_KEY_GRAVE_ACCENT, "`"},
        {GLFW_KEY_LEFT_SHIFT, "Left Shift"},
        {GLFW_KEY_RIGHT_SHIFT, "Right Shift"},
        {GLFW_KEY_LEFT_CONTROL, "Left Ctrl"},
        {GLFW_KEY_RIGHT_CONTROL, "Right Ctrl"},
        {GLFW_KEY_LEFT_ALT, "Left Alt"},
        {GLFW_KEY_RIGHT_ALT, "Right Alt"},
        {GLFW_KEY_LEFT_SUPER, "Left Super"},
        {GLFW_KEY_RIGHT_SUPER, "Right Super"},
        {GLFW_KEY_MENU, "Menu"},
        {GLFW_KEY_HOME, "Home"},
        {GLFW_KEY_END, "End"},
        {GLFW_KEY_PAGE_UP, "Page Up"},
        {GLFW_KEY_PAGE_DOWN, "Page Down"},
        {GLFW_KEY_INSERT, "Insert"},
        {GLFW_KEY_DELETE, "Delete"},
        {GLFW_KEY_CAPS_LOCK, "Caps Lock"},
        {GLFW_KEY_NUM_LOCK, "Num Lock"},
        {GLFW_KEY_SCROLL_LOCK, "Scroll Lock"}
    };

    if (const auto it = kKeyNames.find(b.code); it != kKeyNames.end()) {
        return it->second;
    }

    return std::string("Key ") + std::to_string(b.code);
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

Input::Input(GLFWwindow *window) {
    this->window = window;
    this->inputState = {};

    auto* userPointer = static_cast<GLFWUserPointer*>(glfwGetWindowUserPointer(window));
    userPointer->keyCallback = [this](GLFWwindow* window, int key, int scancode, int action, int mods) {
        this->keyCallback(window, key, scancode, action, mods);
    };
    userPointer->mouseButtonCallback = [this](GLFWwindow* window, int button, int action, int mods) {
        this->mouseButtonCallback(window, button, action, mods);
    };

    glfwSetKeyCallback(window, [](GLFWwindow* w, int key, int scancode, int action, int mods) {
        auto* userPointer = static_cast<GLFWUserPointer*>(glfwGetWindowUserPointer(w));
        userPointer->keyCallback(w, key, scancode, action, mods);
    });

    glfwSetMouseButtonCallback(window, [](GLFWwindow* w, int button, int action, int mods) {
        auto* userPointer = static_cast<GLFWUserPointer*>(glfwGetWindowUserPointer(w));
        if (userPointer->mouseButtonCallback) {
            userPointer->mouseButtonCallback(w, button, action, mods);
        }
    });

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

bool Input::keyMatches(const std::vector<Binding> &binding, int key) const {
    return std::any_of(binding.begin(), binding.end(), [&](const Binding &b) {
        return b.type == Binding::Type::Key && b.code == key;
    });
}

bool Input::mouseMatches(const std::vector<Binding> &binding, int button) const {
    return std::any_of(binding.begin(), binding.end(), [&](const Binding &b) {
        return b.type == Binding::Type::MouseButton && b.code == button;
    });
}

bool Input::isBindingPressed(const std::vector<Binding> &binding) const {
    return std::any_of(binding.begin(), binding.end(), [this](const Binding &b) {
        if (b.type == Binding::Type::Key) {
            return glfwGetKey(window, b.code) == GLFW_PRESS;
        }
        return glfwGetMouseButton(window, b.code) == GLFW_PRESS;
    });
}

void Input::keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (action == GLFW_PRESS) {
        if (keyMatches(keyBindings.fire, key)) {
            inputState.fire = true;
        }

        if (keyMatches(keyBindings.spawn, key)) {
            inputState.spawn = true;
        }

        if (keyMatches(keyBindings.quickQuit, key)) {
            inputState.quickQuit = true;
        }

        if (keyMatches(keyBindings.toggleFullscreen, key)) {
            inputState.toggleFullscreen = true;
        }

        if (keyMatches(keyBindings.chat, key)) {
            inputState.chat = true;
        }

        if (keyMatches(keyBindings.escape, key)) {
            inputState.escape = true;
        }
    }
}

void Input::mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    if (action == GLFW_PRESS) {
        if (mouseMatches(keyBindings.fire, button)) {
            inputState.fire = true;
        }

        if (mouseMatches(keyBindings.spawn, button)) {
            inputState.spawn = true;
        }

        if (mouseMatches(keyBindings.quickQuit, button)) {
            inputState.quickQuit = true;
        }

        if (mouseMatches(keyBindings.toggleFullscreen, button)) {
            inputState.toggleFullscreen = true;
        }

        if (mouseMatches(keyBindings.chat, button)) {
            inputState.chat = true;
        }

        if (mouseMatches(keyBindings.escape, button)) {
            inputState.escape = true;
        }
    }
}

void Input::update() {
    inputState = { 0 };

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

    glfwPollEvents();
}

const InputState &Input::getInputState() const {
    return inputState;
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