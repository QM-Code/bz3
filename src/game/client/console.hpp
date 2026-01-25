#pragma once
#include <string>
#include <vector>

class Game;

class Console {
private:
    Game &game;
    
    struct ChatMsg {
        std::string playerName;
        std::string text;
    };

    std::vector<struct ChatMsg> messages;
    bool chatInFocus = false;

public:
    Console(Game &game);
    ~Console() = default;
    
    void update();
    bool isChatInFocus() const { return chatInFocus; }
    void focusChatInput();
};