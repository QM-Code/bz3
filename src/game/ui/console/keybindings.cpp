#include "ui/console/keybindings.hpp"

namespace ui::bindings {
namespace {

constexpr BindingDefinition kDefinitions[] = {
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
    {"quickQuit", "Quick Quit"}
};

} // namespace

std::span<const BindingDefinition> Definitions() {
    return std::span<const BindingDefinition>(kDefinitions);
}

} // namespace ui::bindings
