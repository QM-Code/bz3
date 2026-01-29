#include "engine/server_engine.hpp"
#include "core/types.hpp"
#include "game/net/messages.hpp"
#include "common/config_store.hpp"
#include <functional>

ServerEngine::ServerEngine(uint16_t serverPort) {
    network = new ServerNetwork(serverPort);
    physics = new PhysicsWorld();
}

ServerEngine::~ServerEngine() {
    delete network;
    delete physics;
}

void ServerEngine::earlyUpdate(TimeUtils::duration deltaTime) {
    network->update();
}

void ServerEngine::lateUpdate(TimeUtils::duration deltaTime) {
    physics->update(deltaTime);
    network->flushPeekedMessages();
    bz::config::ConfigStore::Tick();
}
