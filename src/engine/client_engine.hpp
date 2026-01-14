#pragma once
#include "engine/types.hpp"
#include "engine/components/client_network.hpp"
#include "engine/components/render.hpp"
#include "engine/physics/physics_world.hpp"
#include "engine/components/input.hpp"
#include "engine/components/gui.hpp"
#include "engine/components/audio.hpp"
#include "engine/components/particle_effect_system.hpp"
#include "engine/user_pointer.hpp"
#include <string>

class ClientEngine {
private:
    GLFWwindow *window;
    GLFWUserPointer *userPointer;

public:
    ClientNetwork *network;
    Render *render;
    PhysicsWorld *physics;
    Input *input;
    GUI *gui;
    Audio *audio;
    ParticleEngine *particles;

    ClientEngine(GLFWwindow *window);
    ~ClientEngine();

    void earlyUpdate(TimeUtils::duration deltaTime);
    void step(TimeUtils::duration deltaTime);
    void lateUpdate(TimeUtils::duration deltaTime);
};