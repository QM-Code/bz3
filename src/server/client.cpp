#include "client.hpp"
#include "spdlog/spdlog.h"
#include "game.hpp"

Client::Client(Game &game, client_id id, std::string ip, std::string name) : game(game), id(id), ip(std::move(ip)) {
    state.name = std::move(name);
    state.position = glm::vec3(0.0f, 0.0f, 0.0f);
    state.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    state.velocity = glm::vec3(0.0f, 0.0f, 0.0f);
    state.alive = false;
    state.score = 0;

    state.params = game.world->getDefaultPlayerParameters();

    // Announce this player to all other clients
    ServerMsg_PlayerJoin announceMsg;
    announceMsg.clientId = id;
    announceMsg.state = state;
    game.engine.network->sendExcept<ServerMsg_PlayerJoin>(id, &announceMsg);
}

Client::~Client() {
    ServerMsg_PlayerLeave serverDisconnMsg;
    serverDisconnMsg.clientId = id;
    game.engine.network->sendExcept<ServerMsg_PlayerLeave>(id, &serverDisconnMsg);
}

bool Client::isEqual(client_id cid) const {
    return cid == id;
}

std::string Client::getName() const {
    return state.name;
}

void Client::update() {
    for (const auto &locMsg : game.engine.network->consumeMessages<ClientMsg_PlayerLocation>([this](const ClientMsg_PlayerLocation &msg) {
        return msg.clientId == id;
    })) {
        state.position = locMsg.position;
        state.rotation = locMsg.rotation;

        ServerMsg_PlayerLocation updateMsg;
        updateMsg.clientId = id;
        updateMsg.position = state.position;
        updateMsg.rotation = state.rotation;
        game.engine.network->sendExcept<ServerMsg_PlayerLocation>(id, &updateMsg);
    }

    for (const auto &spawnMsg : game.engine.network->consumeMessages<ClientMsg_RequestPlayerSpawn>([this](const ClientMsg_RequestPlayerSpawn &msg) {
        return msg.clientId == id;
    })) {
        if (state.alive) {
            spdlog::warn("Client::update: Client id {} requested spawn while already alive", id);
            continue;
        }

        Location spawnLocation = game.world->getSpawnLocation();
        state.position = spawnLocation.position;
        state.rotation = spawnLocation.rotation;
        state.velocity = glm::vec3(0.0f);

        ServerMsg_PlayerSpawn spawnRespMsg;
        spawnRespMsg.clientId = id;
        spawnRespMsg.position = state.position;
        spawnRespMsg.rotation = state.rotation;
        spawnRespMsg.velocity = state.velocity;
        game.engine.network->sendAll<ServerMsg_PlayerSpawn>(&spawnRespMsg);

        state.alive = true;
    }
}

void Client::die() {
    if (state.alive) {
        state.alive = false;

        // Broadcast to everyone else
        ServerMsg_PlayerDeath deathMsg;
        deathMsg.clientId = id;
        game.engine.network->sendAll<ServerMsg_PlayerDeath>(&deathMsg);
    }
}

void Client::setScore(int newScore) {
    state.score = newScore;

    ServerMsg_SetScore msg;
    msg.clientId = id;
    msg.score = newScore;
    game.engine.network->sendAll<ServerMsg_SetScore>(&msg);
}

bool Client::setParameter(const std::string &param, float value) {
    if (state.params.find(param) == state.params.end()) {
        spdlog::warn("Client::setParameter: Client id {} attempted to set unknown parameter '{}'", id, param);
        return false;
    }

    state.params[param] = value;

    // Broadcast updated parameters to all clients
    ServerMsg_PlayerParameters paramMsg;
    paramMsg.clientId = id;
    paramMsg.params[param] = value;
    game.engine.network->sendAll<ServerMsg_PlayerParameters>(&paramMsg);
    return true;
}
