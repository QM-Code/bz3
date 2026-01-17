#include "game.hpp"
#include "spdlog/spdlog.h"
#include <algorithm>
#include <utility>

void Game::addClient(std::unique_ptr<Client> client) {
    clients.push_back(std::move(client));
}

void Game::removeClient(client_id id) {
    clients.erase(
        std::remove_if(
            clients.begin(),
            clients.end(),
            [id](const std::unique_ptr<Client> &c) {
                if (c->isEqual(id)) {
                    return true;
                } else {
                    return false;
                }
            }
        ),
        clients.end()
    );
}

Client *Game::getClient(client_id id) {
    for (const auto &client : clients) {
        if (client->isEqual(id)) {
            return client.get();
        }
    }
    return nullptr;
}

Client *Game::getClientByName(const std::string &name) {
    for (const auto &client : clients) {
        if (client->isEqual(name)) {
            return client.get();
        }
    }
    return nullptr;
}

Game::Game(ServerEngine &engine,
           std::string serverName,
           std::string worldName,
           nlohmann::json worldConfig,
           std::string worldDir,
           bool enableWorldZipping)
    : engine(engine) {
    world = new World(*this,
                      std::move(serverName),
                      std::move(worldName),
                      std::move(worldConfig),
                      std::move(worldDir),
                      enableWorldZipping);
    chat = new Chat(*this);
}

Game::~Game() {
    clients.clear();

    shots.clear();

    delete world;
    delete chat;
}

void Game::update(TimeUtils::duration deltaTime) {
    for (const auto &connMsg : engine.network->consumeMessages<ClientMsg_PlayerJoin>()) {
        spdlog::debug("Game::update: New client connection with id {} from IP {}",
                      connMsg.clientId,
                      connMsg.ip);
        if (getClientByName(connMsg.name)) {
            engine.network->disconnectClient(connMsg.clientId, "Client ID already in use.");
            continue;
        }

        if (connMsg.protocolVersion != NET_PROTOCOL_VERSION) {
            spdlog::warn("Game::update: Client id {} protocol mismatch (client {}, server {})",
                         connMsg.clientId,
                         connMsg.protocolVersion,
                         NET_PROTOCOL_VERSION);
            engine.network->disconnectClient(connMsg.clientId, "Protocol version mismatch.");
            continue;
        }

        world->sendInitToClient(connMsg.clientId);
        auto newClient = std::make_unique<Client>(
            *this,
            connMsg.clientId,
            connMsg.ip,
            connMsg.name,
            connMsg.registeredUser,
            connMsg.communityAdmin,
            connMsg.localAdmin);

        // Send existing players to the newcomer
        for (const auto &client : clients) {
            ServerMsg_PlayerJoin existingMsg;
            existingMsg.clientId = client->getId();
            existingMsg.state = client->getState();
            engine.network->send<ServerMsg_PlayerJoin>(connMsg.clientId, &existingMsg);
        }

        addClient(std::move(newClient));
    }

    for (const auto &disconnMsg : engine.network->consumeMessages<ClientMsg_PlayerLeave>()) {
        spdlog::info("Game::update: Client with id {} disconnected", disconnMsg.clientId);
        removeClient(disconnMsg.clientId);
    }

    for (const auto &chatMsg : engine.network->consumeMessages<ClientMsg_Chat>()) {
        chat->handleMessage(chatMsg);
    }

    for (const auto &locMsg : engine.network->consumeMessages<ClientMsg_PlayerLocation>()) {
        Client *client = getClient(locMsg.clientId);
        if (!client) {
            continue;
        }

        client->applyLocation(locMsg.position, locMsg.rotation);
    }

    for (const auto &spawnMsg : engine.network->consumeMessages<ClientMsg_RequestPlayerSpawn>()) {
        Client *client = getClient(spawnMsg.clientId);
        if (!client) {
            continue;
        }

        client->trySpawn(world->getSpawnLocation());
    }

    for (const auto &shotMsg : engine.network->consumeMessages<ClientMsg_CreateShot>()) {
        shots.push_back(std::make_unique<Shot>(
            *this,
            shotMsg.clientId,
            shotMsg.localShotId,
            shotMsg.position,
            shotMsg.velocity
        ));
    }

    for (auto it = shots.begin(); it != shots.end(); ) {
        Shot *shot = it->get();
        shot->update(deltaTime);

        bool expired = shot->isExpired();
        bool hit = false;
        if (!expired) {
            for (const auto &client : clients) {
                if (client->getState().alive == false) {
                    continue;
                }

                if (shot->hits(client.get())) {
                    client_id victimId = client->getId();
                    client_id killerId = shot->getOwnerId();

                    // Apply authoritative score changes
                    if (Client* killer = getClient(killerId)) {
                        if (killerId != victimId) {
                            killer->setScore(killer->getScore() + 1);
                        }
                    }

                    client->setScore(client->getScore() - 1);

                    client->die();
                    hit = true;
                    break;
                }
            }
        }

        if (expired || hit) {
            it = shots.erase(it);
        } else {
            ++it;
        }
    }

    world->update();
}
