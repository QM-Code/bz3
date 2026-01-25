#pragma once

#include <array>
#include <string_view>

namespace game_input {

inline constexpr std::string_view kActionFire = "fire";
inline constexpr std::string_view kActionSpawn = "spawn";
inline constexpr std::string_view kActionJump = "jump";
inline constexpr std::string_view kActionQuickQuit = "quickQuit";
inline constexpr std::string_view kActionChat = "chat";
inline constexpr std::string_view kActionEscape = "escape";
inline constexpr std::string_view kActionToggleFullscreen = "toggleFullscreen";
inline constexpr std::string_view kActionMoveLeft = "moveLeft";
inline constexpr std::string_view kActionMoveRight = "moveRight";
inline constexpr std::string_view kActionMoveForward = "moveForward";
inline constexpr std::string_view kActionMoveBackward = "moveBackward";

constexpr std::array<std::string_view, 11> kActionIds = {
    kActionFire,
    kActionSpawn,
    kActionJump,
    kActionQuickQuit,
    kActionChat,
    kActionEscape,
    kActionToggleFullscreen,
    kActionMoveLeft,
    kActionMoveRight,
    kActionMoveForward,
    kActionMoveBackward
};

} // namespace game_input
