#include "client.hpp"
#include "engine/types.hpp"
#include "game.hpp"
#include "spdlog/spdlog.h"
#include <glm/glm.hpp>

Client::Client(Game &game, client_id id, const PlayerState &initialState) : game(game), id(id) {
    renderId = game.engine.render->create(game.world->getAssetPath("playerModel").string());

    state = initialState;
    justSpawned = state.alive;
    lastSpawnPosition = state.position;
    spdlog::trace("Client::Client: Initialized location for client id {}", id);

    game.engine.render->setPosition(renderId, state.position + glm::vec3(0.0f, -0.9f, 0.0f));
    game.engine.render->setRotation(renderId, state.rotation * glm::angleAxis(glm::pi<float>(), glm::vec3(0, 1, 0)));
    game.engine.render->setScale(renderId, glm::vec3(0.5f, 0.5f, 0.5f));
    game.engine.render->setVisible(renderId, state.alive);
}

Client::~Client() {
    game.engine.render->destroy(renderId);
}

void Client::update() {
    // Listen for incoming world setting changes
    if (state.alive) {
        game.engine.render->setPosition(renderId, state.position + glm::vec3(0.0f, -0.8f, 0.0f));
        game.engine.render->setRotation(renderId, state.rotation * glm::angleAxis(glm::pi<float>(), glm::vec3(0, 1, 0)));
    } else {
        (void)0;
    }
}

void Client::applyState(const PlayerState &nextState) {
    state = nextState;
}

void Client::applyLocation(const ServerMsg_PlayerLocation &msg) {
    if (!state.alive) {
        return; // ignore stale updates while dead
    }

    if (justSpawned) {
        const float snapTolerance = 0.5f;
        if (glm::distance(msg.position, lastSpawnPosition) > snapTolerance) {
            // Skip locations from the previous life to avoid visible snaps
            return;
        }
        justSpawned = false;
    }

    state.position = msg.position;
    state.rotation = msg.rotation;
    state.velocity = msg.velocity;
    spdlog::trace("Client::update: Updated location for client id {}", id);
}

void Client::handleDeath() {
    if (!state.alive) {
        return;
    }
    state.alive = false;
    game.engine.render->setVisible(renderId, false);
    spdlog::trace("Client::update: Client id {} has died", id);
}

void Client::handleSpawn(const ServerMsg_PlayerSpawn &msg) {
    state.position = msg.position;
    state.rotation = msg.rotation;
    state.velocity = msg.velocity;
    state.alive = true;
    justSpawned = true;
    lastSpawnPosition = state.position;
    game.engine.render->setPosition(renderId, state.position + glm::vec3(0.0f, -0.8f, 0.0f));
    game.engine.render->setRotation(renderId, state.rotation * glm::angleAxis(glm::pi<float>(), glm::vec3(0, 1, 0)));
    game.engine.render->setVisible(renderId, true);
    spdlog::trace("Client::update: Client id {} has spawned", id);
}

    void Client::applySetScore(int score) {
        state.score = score;
    }

bool Client::isEqual(client_id otherId) {
    return this->id == otherId;
}
