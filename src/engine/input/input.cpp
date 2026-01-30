#include "input/input.hpp"

#include "platform/window.hpp"
#include "common/config_store.hpp"

#include <optional>
#include <spdlog/spdlog.h>

Input::Input(platform::Window &window, input::InputMap::DefaultBindingsMap defaultBindings) {
    this->window = &window;
    defaultBindings_ = std::move(defaultBindings);
    loadKeyBindings();
}

void Input::loadKeyBindings() {
    std::optional<bz::json::Value> keybindingsConfig;

    if (auto keybindingsOpt = bz::config::ConfigStore::GetCopy("keybindings")) {
        if (keybindingsOpt->is_object()) {
            keybindingsConfig = std::move(keybindingsOpt);
        } else {
            spdlog::warn("Input: 'keybindings' exists but is not a JSON object; falling back to defaults");
        }
    }

    const bz::json::Value *keybindingsJson = keybindingsConfig ? &(*keybindingsConfig) : nullptr;
    mapper_.loadBindings(keybindingsJson, defaultBindings_);
}

void Input::update(const std::vector<platform::Event> &events) {
    lastEvents_ = events;
}

bool Input::actionTriggered(std::string_view actionId) const {
    return mapper_.actionTriggered(actionId, lastEvents_);
}

bool Input::actionDown(std::string_view actionId) const {
    return mapper_.actionDown(actionId, window);
}

void Input::reloadKeyBindings() {
    loadKeyBindings();
}

std::string Input::bindingListDisplay(std::string_view actionId) const {
    return mapper_.bindingListDisplay(actionId);
}
