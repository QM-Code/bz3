#include "ui/console/keybindings.hpp"

namespace ui::bindings {
namespace {

constexpr BindingDefinition kDefinitions[] = {
    {nullptr, "Gameplay", true},
    {"moveForward", "Move Forward"},
    {"moveBackward", "Move Backward"},
    {"moveLeft", "Move Left"},
    {"moveRight", "Move Right"},
    {"jump", "Jump"},
    {"fire", "Fire"},
    {"spawn", "Spawn"},
    {"chat", "Chat"},
    {"toggleFullscreen", "Toggle Fullscreen"},
    {"escape", "Escape Menu"},
    {"quickQuit", "Quick Quit"},
    {nullptr, "Roaming", true},
    {"roamMoveForward", "Roam Camera Forward"},
    {"roamMoveBackward", "Roam Camera Backward"},
    {"roamMoveLeft", "Roam Camera Left"},
    {"roamMoveRight", "Roam Camera Right"},
    {"roamMoveUp", "Roam Camera Up"},
    {"roamMoveDown", "Roam Camera Down"},
    {"roamLook", "Roam Camera Look (Hold)"},
    {"roamMoveFast", "Roam Camera Fast"}
};

} // namespace

std::span<const BindingDefinition> Definitions() {
    return std::span<const BindingDefinition>(kDefinitions);
}

} // namespace ui::bindings
