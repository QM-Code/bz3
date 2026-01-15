#pragma once
#include <cstdint>
#include "engine/types.hpp"

class Game;

class Client {
private:
    Game &game;
    client_id id;
    render_id renderId;

    PlayerState state;

public:
    Client(Game &game, client_id id, const PlayerState &initialState);
    ~Client();

    void update();
    void applyState(const PlayerState &nextState);
    void applyLocation(const ServerMsg_PlayerLocation &msg);
    void handleDeath();
    void handleSpawn(const ServerMsg_PlayerSpawn &msg);
    void applySetScore(int score);
    bool isEqual(client_id otherId);
    std::string getName() const { return state.name; }
    int getScore() const { return state.score; }
};
