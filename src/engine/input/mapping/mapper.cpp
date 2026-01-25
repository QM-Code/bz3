#include "input/mapping/mapper.hpp"

#include "platform/window.hpp"

#include <algorithm>

namespace input {
namespace {

bool MatchesKey(const std::vector<Binding>& bindings, platform::Key key) {
    return std::any_of(bindings.begin(), bindings.end(), [&](const Binding &b) {
        return b.type == Binding::Type::Key && b.key == key;
    });
}

bool MatchesMouse(const std::vector<Binding>& bindings, platform::MouseButton button) {
    return std::any_of(bindings.begin(), bindings.end(), [&](const Binding &b) {
        return b.type == Binding::Type::MouseButton && b.mouseButton == button;
    });
}

} // namespace

void InputMapper::loadBindings(const bz::json::Value* keybindings) {
    map_.load(keybindings);
}

bool InputMapper::actionTriggered(Action action, const std::vector<platform::Event>& events) const {
    const auto &bindings = map_.bindings(action);
    for (const auto &event : events) {
        if (event.type == platform::EventType::KeyDown) {
            if (MatchesKey(bindings, event.key)) {
                return true;
            }
        } else if (event.type == platform::EventType::MouseButtonDown) {
            if (MatchesMouse(bindings, event.mouseButton)) {
                return true;
            }
        }
    }
    return false;
}

bool InputMapper::actionDown(Action action, const platform::Window* window) const {
    if (!window) {
        return false;
    }
    const auto &bindings = map_.bindings(action);
    for (const auto &binding : bindings) {
        if (binding.type == Binding::Type::Key) {
            if (window->isKeyDown(binding.key)) {
                return true;
            }
        } else if (binding.type == Binding::Type::MouseButton) {
            if (window->isMouseDown(binding.mouseButton)) {
                return true;
            }
        }
    }
    return false;
}

} // namespace input
