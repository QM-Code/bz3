#pragma once
#include "core/types.hpp"
#include "game/net/messages.hpp"
#include "game/net/client_network.hpp"
#include "renderer/render.hpp"
#include "physics/physics_world.hpp"
#include "input/input.hpp"
#include "ui/system.hpp"
#include "audio/audio.hpp"
#include "renderer/particle_effect_system.hpp"
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

public:
    ClientNetwork *network;
    Render *render;
    PhysicsWorld *physics;
    Input *input;
    UiSystem *ui;
    Audio *audio;
    ParticleEngine *particles;

    ClientEngine(platform::Window &window);
    ~ClientEngine();

    void earlyUpdate(TimeUtils::duration deltaTime);
    void step(TimeUtils::duration deltaTime);
    void lateUpdate(TimeUtils::duration deltaTime);
};
