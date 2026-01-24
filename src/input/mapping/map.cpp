#include "input/mapping/map.hpp"

#include <algorithm>
#include <spdlog/spdlog.h>

namespace input {
namespace {

constexpr std::size_t ActionIndex(Action action) {
    return static_cast<std::size_t>(action);
}

std::vector<Binding> ParseBindings(const bz::json::Value* keybindings,
                                  Action action,
                                  std::initializer_list<const char*> defaults) {
    std::vector<Binding> bindings;

    auto addBinding = [&](const std::string& name) {
        if (auto binding = BindingFromName(name)) {
            if (std::none_of(bindings.begin(), bindings.end(), [&](const Binding &b) {
                    return b.type == binding->type && b.key == binding->key && b.mouseButton == binding->mouseButton;
                })) {
                bindings.push_back(*binding);
            }
        } else {
            spdlog::warn("Input: Unknown key '{}' for action '{}'", name, ActionName(action));
        }
    };

    if (keybindings) {
        const auto it = keybindings->find(ActionName(action));
        if (it != keybindings->end()) {
            if (!it->is_array()) {
                spdlog::warn("Input: keybindings.{} must be an array of strings", ActionName(action));
            } else {
                for (const auto &value : *it) {
                    if (value.is_string()) {
                        addBinding(value.get<std::string>());
                    } else {
                        spdlog::warn("Input: keybindings.{} entries must be strings", ActionName(action));
                    }
                }
            }
        }
    }

    if (bindings.empty()) {
        for (const auto *name : defaults) {
            addBinding(name);
        }
    }

    return bindings;
}

} // namespace

void InputMap::load(const bz::json::Value* keybindings) {
    bindings_[ActionIndex(Action::Fire)] = ParseBindings(keybindings, Action::Fire, {"F", "E", "LEFT_MOUSE"});
    bindings_[ActionIndex(Action::Spawn)] = ParseBindings(keybindings, Action::Spawn, {"U"});
    bindings_[ActionIndex(Action::Jump)] = ParseBindings(keybindings, Action::Jump, {"SPACE"});
    bindings_[ActionIndex(Action::QuickQuit)] = ParseBindings(keybindings, Action::QuickQuit, {"F12"});
    bindings_[ActionIndex(Action::Chat)] = ParseBindings(keybindings, Action::Chat, {"T"});
    bindings_[ActionIndex(Action::Escape)] = ParseBindings(keybindings, Action::Escape, {"ESCAPE"});
    bindings_[ActionIndex(Action::ToggleFullscreen)] = ParseBindings(keybindings, Action::ToggleFullscreen, {"RIGHT_BRACKET"});
    bindings_[ActionIndex(Action::MoveLeft)] = ParseBindings(keybindings, Action::MoveLeft, {"LEFT", "J"});
    bindings_[ActionIndex(Action::MoveRight)] = ParseBindings(keybindings, Action::MoveRight, {"RIGHT", "L"});
    bindings_[ActionIndex(Action::MoveForward)] = ParseBindings(keybindings, Action::MoveForward, {"UP", "I"});
    bindings_[ActionIndex(Action::MoveBackward)] = ParseBindings(keybindings, Action::MoveBackward, {"DOWN", "K"});
}

const std::vector<Binding>& InputMap::bindings(Action action) const {
    return bindings_[ActionIndex(action)];
}

std::string InputMap::bindingListDisplay(Action action) const {
    const auto &list = bindings(action);
    if (list.empty()) {
        return "U";
    }
    return JoinBindingStrings(list);
}

} // namespace input
