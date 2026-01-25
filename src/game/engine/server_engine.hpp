#pragma once
#include "core/types.hpp"
#include "game/net/messages.hpp"
#include "game/net/server_network.hpp"
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
