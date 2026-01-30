#include "input/mapping/map.hpp"

#include <algorithm>
#include <spdlog/spdlog.h>

namespace input {
namespace {

std::vector<Binding> ParseBindings(const karma::json::Value* keybindings,
                                   std::string_view actionId,
                                   const std::vector<std::string>& defaults) {
    std::vector<Binding> bindings;

    auto addBinding = [&](const std::string& name) {
        if (auto binding = BindingFromName(name)) {
            if (std::none_of(bindings.begin(), bindings.end(), [&](const Binding &b) {
                    return b.type == binding->type && b.key == binding->key && b.mouseButton == binding->mouseButton;
                })) {
                bindings.push_back(*binding);
            }
        } else {
            spdlog::warn("Input: Unknown key '{}' for action '{}'", name, actionId);
        }
    };

    if (keybindings) {
        const auto it = keybindings->find(std::string(actionId));
        if (it != keybindings->end()) {
            if (!it->is_array()) {
                spdlog::warn("Input: keybindings.{} must be an array of strings", actionId);
            } else {
                for (const auto &value : *it) {
                    if (value.is_string()) {
                        addBinding(value.get<std::string>());
                    } else {
                        spdlog::warn("Input: keybindings.{} entries must be strings", actionId);
                    }
                }
            }
        }
    }

    if (bindings.empty()) {
        for (const auto &name : defaults) {
            addBinding(name);
        }
    }

    return bindings;
}

} // namespace

void InputMap::load(const karma::json::Value* keybindings, const DefaultBindingsMap& defaults) {
    bindings_.clear();
    for (const auto& [actionId, defaultList] : defaults) {
        bindings_[actionId] = ParseBindings(keybindings, actionId, defaultList);
    }
}

const std::vector<Binding>& InputMap::bindings(std::string_view actionId) const {
    static const std::vector<Binding> kEmptyBindings;
    auto it = bindings_.find(std::string(actionId));
    if (it == bindings_.end()) {
        return kEmptyBindings;
    }
    return it->second;
}

std::string InputMap::bindingListDisplay(std::string_view actionId) const {
    const auto &list = bindings(actionId);
    if (list.empty()) {
        return "Unbound";
    }
    return JoinBindingStrings(list);
}

} // namespace input
