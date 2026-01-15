#pragma once
#include <string>
#include "engine/types.hpp"

class Game;

class Client {
private:
    Game &game;
    
    bool initialized;
    std::string ip;
    client_id id;

    PlayerState state;

public:
    Client(Game &game, client_id id, std::string ip);
    ~Client();

    bool isEqual(client_id cid) const;
    bool isEqual(const std::string &name) const { return state.name == name; }
    std::string getName() const;
    std::string getIP() const { return ip; }
    client_id getId() const { return id; }
    bool isInitialized() const { return initialized; }
    const PlayerState &getState() const { return state; }
    int getScore() const { return state.score; }
    void update();
    glm::vec3 getPosition() const { return state.position; }
    void die();
    void setScore(int newScore);
    bool setParameter(const std::string &param, float value);
};
