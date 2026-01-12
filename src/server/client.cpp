#include "client.hpp"
#include "spdlog/spdlog.h"
#include "game.hpp"

Client::Client(Game &game, client_id id, std::string ip) : game(game), id(id), ip(ip) { 
    initialized = false;

    state.name = "empty";
    state.position = glm::vec3(0.0f, 0.0f, 0.0f);
    state.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    state.velocity = glm::vec3(0.0f, 0.0f, 0.0f);
    state.alive = false;

    state.params = game.world->getDefaultPlayerParameters();
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
    for (const auto &initMsg : game.engine.network->consumeMessages<ClientMsg_Init>([this](const ClientMsg_Init &msg) {
        return msg.clientId == id;
    })) {
        if (initialized) {
            spdlog::warn("Client::update: Client id {} sent duplicate initialization message", id);
            continue;
        }

        if (initMsg.protocolVersion != NET_PROTOCOL_VERSION) {
            spdlog::warn("Client::update: Client id {} protocol mismatch (client {}, server {})",
                         id,
                         initMsg.protocolVersion,
                         NET_PROTOCOL_VERSION);
            game.engine.network->disconnectClient(id, "Protocol version mismatch.");
            return;
        }

        spdlog::info("Client::update: Client id {} initialized with name {}", id, initMsg.name);
        state.name = initMsg.name;
        initialized = true;

        ServerMsg_PlayerJoin connMsg;
        connMsg.clientId = id;
        connMsg.state = state;
        game.engine.network->sendExcept<ServerMsg_PlayerJoin>(id, &connMsg);
    }

    if (initialized) {
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

        ServerMsg_PlayerSpawn spawnRespMsg;
        spawnRespMsg.clientId = id;
        spawnRespMsg.position = state.position;
        spawnRespMsg.rotation = state.rotation;
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
