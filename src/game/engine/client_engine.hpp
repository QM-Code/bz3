#pragma once
#include "core/types.hpp"
#include "game/net/messages.hpp"
#include "game/net/client_network.hpp"
#include "renderer/render.hpp"
#include "physics/physics_world.hpp"
#include "input/input.hpp"
#include "game/input/state.hpp"
#include "ui/system.hpp"
#include "audio/audio.hpp"
#include "platform/window.hpp"
#include <string>
#include <memory>

namespace ui {
class RenderBridge;
}

class ClientEngine {
private:
    platform::Window *window;
    std::unique_ptr<ui::RenderBridge> uiRenderBridge;
    std::string lastLanguage;

public:
    ClientNetwork *network;
    Render *render;
    PhysicsWorld *physics;
    Input *input;
    game_input::InputState inputState;
    UiSystem *ui;
    Audio *audio;

    ClientEngine(platform::Window &window);
    ~ClientEngine();

    void earlyUpdate(TimeUtils::duration deltaTime);
    void step(TimeUtils::duration deltaTime);
    void lateUpdate(TimeUtils::duration deltaTime);

    const game_input::InputState& getInputState() const { return inputState; }
};
