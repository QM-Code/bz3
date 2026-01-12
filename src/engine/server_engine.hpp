#pragma once
#include "engine/types.hpp"
#include "engine/components/server_network.hpp"
#include "engine/physics/physics_world.hpp"

class ServerEngine {
public:
    ServerNetwork *network;
    PhysicsWorld *physics;

    ServerEngine(uint16_t serverPort);
    ~ServerEngine();

    void earlyUpdate(TimeUtils::duration deltaTime);
    void lateUpdate(TimeUtils::duration deltaTime);
};