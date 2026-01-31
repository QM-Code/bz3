#pragma once
#include "karma/core/types.hpp"
#include "game/net/messages.hpp"

class Game;
class Client;

class Shot {
private:
    Game &game;
    client_id ownerId;
    shot_id localId;
    shot_id globalId;
    glm::vec3 position;
    glm::vec3 velocity;
    TimeUtils::time creationTime;

    shot_id getNextGlobalShotId() {
        static shot_id nextId = 1;
        return nextId++;
    }

public:
    Shot(Game &game, client_id ownerId, shot_id localShotId, glm::vec3 position, glm::vec3 velocity);
    ~Shot();

    void update(TimeUtils::duration deltaTime);
    bool hits(Client *client);
    bool isExpired() const;
    client_id getOwnerId() const { return ownerId; }
    shot_id getGlobalId() const { return globalId; }
};
