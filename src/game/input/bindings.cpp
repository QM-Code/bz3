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
        {std::string(kActionMoveBackward), {"DOWN", "K"}},
        {std::string(kActionRoamMoveLeft), {}},
        {std::string(kActionRoamMoveRight), {}},
        {std::string(kActionRoamMoveForward), {}},
        {std::string(kActionRoamMoveBackward), {}},
        {std::string(kActionRoamMoveUp), {}},
        {std::string(kActionRoamMoveDown), {}},
        {std::string(kActionRoamLook), {}},
        {std::string(kActionRoamMoveFast), {}}
    };
}

} // namespace game_input
