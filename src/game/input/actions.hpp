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
inline constexpr std::string_view kActionRoamMoveLeft = "roamMoveLeft";
inline constexpr std::string_view kActionRoamMoveRight = "roamMoveRight";
inline constexpr std::string_view kActionRoamMoveForward = "roamMoveForward";
inline constexpr std::string_view kActionRoamMoveBackward = "roamMoveBackward";
inline constexpr std::string_view kActionRoamMoveUp = "roamMoveUp";
inline constexpr std::string_view kActionRoamMoveDown = "roamMoveDown";
inline constexpr std::string_view kActionRoamLook = "roamLook";
inline constexpr std::string_view kActionRoamMoveFast = "roamMoveFast";

constexpr std::array<std::string_view, 19> kActionIds = {
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
    kActionMoveBackward,
    kActionRoamMoveLeft,
    kActionRoamMoveRight,
    kActionRoamMoveForward,
    kActionRoamMoveBackward,
    kActionRoamMoveUp,
    kActionRoamMoveDown,
    kActionRoamLook,
    kActionRoamMoveFast
};

} // namespace game_input
