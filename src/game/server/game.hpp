#pragma once
#include "karma/core/types.hpp"
#include "game/net/messages.hpp"
#include "game/engine/server_engine.hpp"
#include "client.hpp"
#include "shot.hpp"
#include "world_session.hpp"
#include "chat.hpp"
#include <vector>
#include <memory>

class Game {
private:
    std::vector<std::unique_ptr<Client>> clients;
    void addClient(std::unique_ptr<Client> client);
    void removeClient(client_id id);

    std::vector<std::unique_ptr<Shot>> shots;

    client_id getNextClientId() {
        static client_id nextId = 4;
        return nextId++;
    }

public:
    ServerEngine &engine;
    ServerWorldSession *world;
    Chat *chat;

    const std::vector<std::unique_ptr<Client>> &getClients() const { return clients; }
    Client *getClient(client_id id);
    Client *getClientByName(const std::string &name);
    

        Game(class ServerEngine &engine,
            std::string serverName,
            std::string worldName,
            karma::json::Value worldConfig,
            std::string worldDir,
            bool enableWorldZipping);
    ~Game();

    void update(TimeUtils::duration deltaTime);
};
