#include "input/mapping/binding.hpp"

#include <algorithm>
#include <cctype>
#include <unordered_map>

namespace input {
namespace {

std::string NormalizeKeyName(std::string name) {
    std::transform(name.begin(), name.end(), name.begin(), [](unsigned char ch) {
        if (ch == ' ' || ch == '-') {
            return '_';
        }
        return static_cast<char>(std::toupper(ch));
    });
    return name;
}

} // namespace

std::optional<Binding> BindingFromName(std::string_view nameView) {
    std::string name = NormalizeKeyName(std::string(nameView));

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

std::string BindingToString(const Binding& binding) {
    if (binding.type == Binding::Type::MouseButton) {
        switch (binding.mouseButton) {
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

    if (binding.key >= platform::Key::F1 && binding.key <= platform::Key::F25) {
        return "F" + std::to_string(1 + static_cast<int>(binding.key) - static_cast<int>(platform::Key::F1));
    }

    if (binding.key >= platform::Key::A && binding.key <= platform::Key::Z) {
        char c = static_cast<char>('A' + (static_cast<int>(binding.key) - static_cast<int>(platform::Key::A)));
        return std::string(1, c);
    }

    if (binding.key >= platform::Key::Num0 && binding.key <= platform::Key::Num9) {
        char c = static_cast<char>('0' + (static_cast<int>(binding.key) - static_cast<int>(platform::Key::Num0)));
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

    if (const auto it = kKeyNames.find(binding.key); it != kKeyNames.end()) {
        return it->second;
    }

    return "Key";
}

std::string JoinBindingStrings(const std::vector<Binding>& bindings) {
    std::string output;
    for (std::size_t i = 0; i < bindings.size(); ++i) {
        if (i > 0) {
            output += " or ";
        }
        output += BindingToString(bindings[i]);
    }
    return output;
}

} // namespace input
