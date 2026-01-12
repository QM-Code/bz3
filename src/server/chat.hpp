#pragma once
#include <vector>
#include <string>

class Game;

class Chat {
private:
    Game &game;
    std::vector<std::string> messages;

    std::vector<std::string> tokenizeCommand(const std::string &message);

public:
    Chat(Game &game);
    ~Chat();

    void update();
};