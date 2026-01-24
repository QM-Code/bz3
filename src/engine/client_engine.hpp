#pragma once
#include "core/types.hpp"
#include "network/client_network.hpp"
#include "render/render.hpp"
#include "physics/physics_world.hpp"
#include "input/input.hpp"
#include "ui/system.hpp"
#include "audio/audio.hpp"
#include "render/particle_effect_system.hpp"
#include "platform/window.hpp"
#include <string>

class ClientEngine {
private:
    platform::Window *window;

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
