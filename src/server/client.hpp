#pragma once
#include <string>
#include "engine/types.hpp"

class Game;

class Client {
private:
    Game &game;
    std::string ip;
    client_id id;
    bool registeredUser = false;
    bool communityAdmin = false;
    bool localAdmin = false;

    PlayerState state;

public:
    Client(Game &game,
           client_id id,
           std::string ip,
           std::string name,
           bool registeredUser,
           bool communityAdmin,
           bool localAdmin);
    ~Client();

    bool isEqual(client_id cid) const;
    bool isEqual(const std::string &name) const { return state.name == name; }
    std::string getName() const;
    std::string getIP() const { return ip; }
    client_id getId() const { return id; }
    const PlayerState &getState() const { return state; }
    int getScore() const { return state.score; }
    bool isRegisteredUser() const { return registeredUser; }
    bool isCommunityAdmin() const { return communityAdmin; }
    bool isLocalAdmin() const { return localAdmin; }
    glm::vec3 getPosition() const { return state.position; }

    void applyLocation(const glm::vec3 &position, const glm::quat &rotation);
    void trySpawn(const Location &spawnLocation);

    void die();
    void setScore(int newScore);
    bool setParameter(const std::string &param, float value);
};
