#include "game/input/bindings.hpp"

namespace game_input {

DefaultBindingsMap DefaultKeybindings() {
    return {
        {std::string(kActionFire), {"F", "E", "LEFT_MOUSE"}},
        {std::string(kActionSpawn), {"U"}},
        {std::string(kActionJump), {"SPACE"}},
        {std::string(kActionQuickQuit), {"F12"}},
        {std::string(kActionChat), {"T"}},
        {std::string(kActionEscape), {"ESCAPE"}},
        {std::string(kActionToggleFullscreen), {"RIGHT_BRACKET"}},
        {std::string(kActionMoveLeft), {"LEFT", "J"}},
        {std::string(kActionMoveRight), {"RIGHT", "L"}},
        {std::string(kActionMoveForward), {"UP", "I"}},
        {std::string(kActionMoveBackward), {"DOWN", "K"}}
    };
}

} // namespace game_input
