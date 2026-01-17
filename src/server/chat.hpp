#pragma once
#include <vector>
#include <string>

#include "engine/types.hpp"

class Game;

class Chat {
private:
    Game &game;
    std::vector<std::string> messages;

    std::vector<std::string> tokenizeCommand(const std::string &message);

public:
    Chat(Game &game);
    ~Chat();

    void handleMessage(const ClientMsg_Chat &chatMsg);
};