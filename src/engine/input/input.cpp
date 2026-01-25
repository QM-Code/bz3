#include "input/input.hpp"

#include "platform/window.hpp"
#include "common/data_path_resolver.hpp"

#include <optional>
#include <spdlog/spdlog.h>

Input::Input(platform::Window &window) {
    this->window = &window;
    this->inputState = {};
    loadKeyBindings();
}

void Input::loadKeyBindings() {
    std::optional<bz::json::Value> keybindingsConfig;

    if (auto keybindingsOpt = bz::data::ConfigValueCopy("keybindings")) {
        if (keybindingsOpt->is_object()) {
            keybindingsConfig = std::move(keybindingsOpt);
        } else {
            spdlog::warn("Input: 'keybindings' exists but is not a JSON object; falling back to defaults");
        }
    }

    const bz::json::Value *keybindingsJson = keybindingsConfig ? &(*keybindingsConfig) : nullptr;
    mapper_.loadBindings(keybindingsJson);
}

void Input::update(const std::vector<platform::Event> &events) {
    inputState = { 0 };

    if (mapper_.actionTriggered(input::Action::Fire, events)) {
        inputState.fire = true;
    }
    if (mapper_.actionTriggered(input::Action::Spawn, events)) {
        inputState.spawn = true;
    }
    if (mapper_.actionTriggered(input::Action::QuickQuit, events)) {
        inputState.quickQuit = true;
    }
    if (mapper_.actionTriggered(input::Action::ToggleFullscreen, events)) {
        inputState.toggleFullscreen = true;
    }
    if (mapper_.actionTriggered(input::Action::Chat, events)) {
        inputState.chat = true;
    }
    if (mapper_.actionTriggered(input::Action::Escape, events)) {
        inputState.escape = true;
    }

    if (mapper_.actionDown(input::Action::MoveLeft, window)) {
        inputState.movement.x -= 1;
    }
    if (mapper_.actionDown(input::Action::MoveRight, window)) {
        inputState.movement.x += 1;
    }

    if (mapper_.actionDown(input::Action::MoveForward, window)) {
        inputState.movement.y += 1;
    }
    if (mapper_.actionDown(input::Action::MoveBackward, window)) {
        inputState.movement.y -= 1;
    }

    if (mapper_.actionDown(input::Action::Jump, window)) {
        inputState.jump = true;
    }
}

const InputState &Input::getInputState() const {
    return inputState;
}

void Input::clearState() {
    inputState = { 0 };
}

void Input::reloadKeyBindings() {
    loadKeyBindings();
}

std::string Input::bindingListDisplay(input::Action action) const {
    return mapper_.bindingListDisplay(action);
}

std::string Input::spawnHintText() const {
    const auto hint = bindingListDisplay(input::Action::Spawn);
    return "Press " + hint + " to spawn";
}
