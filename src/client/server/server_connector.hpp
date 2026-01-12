#pragma once

#include <cstdint>
#include <memory>
#include <string>

class ClientEngine;
class Game;

class ServerConnector {
public:
    ServerConnector(ClientEngine &engine,
                    std::string playerName,
                    std::string worldDir,
                    std::unique_ptr<Game> &game);

    bool connect(const std::string &host, uint16_t port);

private:
    ClientEngine &engine;
    std::unique_ptr<Game> &game;
    std::string playerName;
    std::string worldDir;
};
