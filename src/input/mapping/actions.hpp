#pragma once

#include <array>
#include <optional>
#include <string_view>

namespace input {

enum class Action {
    Fire,
    Spawn,
    Jump,
    QuickQuit,
    Chat,
    Escape,
    ToggleFullscreen,
    MoveLeft,
    MoveRight,
    MoveForward,
    MoveBackward
};

constexpr std::array<Action, 11> kAllActions = {
    Action::Fire,
    Action::Spawn,
    Action::Jump,
    Action::QuickQuit,
    Action::Chat,
    Action::Escape,
    Action::ToggleFullscreen,
    Action::MoveLeft,
    Action::MoveRight,
    Action::MoveForward,
    Action::MoveBackward
};

inline std::string_view ActionName(Action action) {
    switch (action) {
        case Action::Fire: return "fire";
        case Action::Spawn: return "spawn";
        case Action::Jump: return "jump";
        case Action::QuickQuit: return "quickQuit";
        case Action::Chat: return "chat";
        case Action::Escape: return "escape";
        case Action::ToggleFullscreen: return "toggleFullscreen";
        case Action::MoveLeft: return "moveLeft";
        case Action::MoveRight: return "moveRight";
        case Action::MoveForward: return "moveForward";
        case Action::MoveBackward: return "moveBackward";
    }
    return "unknown";
}

inline std::optional<Action> ActionFromString(std::string_view name) {
    if (name == "fire") return Action::Fire;
    if (name == "spawn") return Action::Spawn;
    if (name == "jump") return Action::Jump;
    if (name == "quickQuit") return Action::QuickQuit;
    if (name == "chat") return Action::Chat;
    if (name == "escape") return Action::Escape;
    if (name == "toggleFullscreen") return Action::ToggleFullscreen;
    if (name == "moveLeft") return Action::MoveLeft;
    if (name == "moveRight") return Action::MoveRight;
    if (name == "moveForward") return Action::MoveForward;
    if (name == "moveBackward") return Action::MoveBackward;
    return std::nullopt;
}

} // namespace input
