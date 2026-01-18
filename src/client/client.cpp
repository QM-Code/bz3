#include "client.hpp"
#include "engine/types.hpp"
#include "game.hpp"
#include "spdlog/spdlog.h"
#include <glm/glm.hpp>

void Client::syncRenderFromState() {
    game.engine.render->setPosition(renderId, state.position);
    game.engine.render->setRotation(renderId, state.rotation);
    game.engine.render->setVisible(renderId, state.alive);
}

Client::Client(Game &game, client_id id, const PlayerState &initialState) : Actor(game, id) {
    renderId = game.engine.render->create(game.world->getAssetPath("playerModel").string(), false);
    game.engine.render->setRadarCircleGraphic(renderId, 1.2f);

    setState(initialState);
    justSpawned = state.alive;
    lastSpawnPosition = state.position;
    spdlog::trace("Client::Client: Initialized location for client id {}", id);
}

Client::~Client() {
    game.engine.render->destroy(renderId);
}

void Client::update(TimeUtils::duration /*deltaTime*/) {
    if (!state.alive) {
        return;
    }
    syncRenderFromState();
}

void Client::setState(const PlayerState &newState) {
    state = newState;
}

void Client::die() {
    if (!state.alive) {
        return;
    }
    state.alive = false;
    game.engine.render->setVisible(renderId, false);
    spdlog::trace("Client::update: Client id {} has died", id);
}

void Client::spawn(glm::vec3 position, glm::quat rotation, glm::vec3 velocity) {
    setLocation(position, rotation, velocity);
    state.alive = true;
    justSpawned = true;
    lastSpawnPosition = state.position;
    syncRenderFromState();
    spdlog::trace("Client::update: Client id {} has spawned", id);
}
