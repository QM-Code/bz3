#pragma once
#include "core/types.hpp"
#include "network/server_network.hpp"
#include "physics/physics_world.hpp"

class ServerEngine {
public:
    ServerNetwork *network;
    PhysicsWorld *physics;

    ServerEngine(uint16_t serverPort);
    ~ServerEngine();

    void earlyUpdate(TimeUtils::duration deltaTime);
    void lateUpdate(TimeUtils::duration deltaTime);
};