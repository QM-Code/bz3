#include "game/input/state.hpp"

#include "game/input/actions.hpp"
#include "karma/input/input.hpp"
#include "karma/common/i18n.hpp"

namespace game_input {

InputState BuildInputState(const Input& input) {
    InputState state{};

    if (input.actionTriggered(kActionFire)) {
        state.fire = true;
    }
    if (input.actionTriggered(kActionSpawn)) {
        state.spawn = true;
    }
    if (input.actionTriggered(kActionQuickQuit)) {
        state.quickQuit = true;
    }
    if (input.actionTriggered(kActionToggleFullscreen)) {
        state.toggleFullscreen = true;
    }
    if (input.actionTriggered(kActionChat)) {
        state.chat = true;
    }
    if (input.actionTriggered(kActionEscape)) {
        state.escape = true;
    }

    if (input.actionDown(kActionMoveLeft)) {
        state.movement.x -= 1;
    }
    if (input.actionDown(kActionMoveRight)) {
        state.movement.x += 1;
    }

    if (input.actionDown(kActionMoveForward)) {
        state.movement.y += 1;
    }
    if (input.actionDown(kActionMoveBackward)) {
        state.movement.y -= 1;
    }

    if (input.actionDown(kActionJump)) {
        state.jump = true;
    }

    return state;
}

std::string SpawnHintText(const Input& input) {
    const auto hint = input.bindingListDisplay(kActionSpawn);
    return karma::i18n::Get().format("ui.hud.spawn_hint", {{"binding", hint}});
}

} // namespace game_input
