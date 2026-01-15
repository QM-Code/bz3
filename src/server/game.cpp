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
    chat->update();

    for (const auto &client : clients) {
        client->update();
    }

    for (const auto &connMsg : engine.network->consumeMessages<ClientMsg_PlayerJoin>()) {
        world->sendInitToClient(connMsg.clientId);
        auto newClient = std::make_unique<Client>(*this, connMsg.clientId, connMsg.ip);

        for (const auto &client : clients) {
            if (!client->isInitialized()) {
                continue;
            }
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
