#include "shot.hpp"
#include "game.hpp"
#include "spdlog/spdlog.h"

Shot::Shot(Game &game, client_id ownerId, shot_id localShotId, glm::vec3 position, glm::vec3 velocity) : game(game) {
    this->ownerId = ownerId;
    this->localId = localShotId;
    this->position = position;
    this->velocity = velocity;

    this->globalId = getNextGlobalShotId();

    ServerMsg_CreateShot serverShotMsg;
    serverShotMsg.globalShotId = globalId;
    serverShotMsg.position = position;
    serverShotMsg.velocity = velocity;
    game.engine.network->sendExcept<ServerMsg_CreateShot>(ownerId, &serverShotMsg);

    creationTime = TimeUtils::GetCurrentTime();
}

Shot::~Shot() {
    // Local remove message to owner
    ServerMsg_RemoveShot localRemoveMsg;
    localRemoveMsg.isGlobalId = false;
    localRemoveMsg.shotId = localId;
    game.engine.network->send<ServerMsg_RemoveShot>(ownerId, &localRemoveMsg);

    // Global remove message to everyone else
    ServerMsg_RemoveShot globalRemoveMsg;
    globalRemoveMsg.isGlobalId = true;
    globalRemoveMsg.shotId = globalId;
    game.engine.network->sendExcept<ServerMsg_RemoveShot>(ownerId, &globalRemoveMsg);
}

void Shot::update(TimeUtils::duration deltaTime) {
    glm::vec3 hitPoint, hitNormal;
    if (game.engine.physics->raycast(position, position + velocity * deltaTime, hitPoint, hitNormal)) {
        velocity = glm::reflect(velocity, hitNormal);
    }
    
    position += velocity * deltaTime;
}

bool Shot::hits(Client *client) {
    if (glm::distance(position, client->getPosition() + glm::vec3(0.0f, 1.0f, 0.0f)) < 1.0f) {
        return true;
    } else {
        return false;
    }
}

bool Shot::isExpired() const {
    // TODO: CHANGE THIS TO USE SHOT LIFETIME FROM CURRENT SHOT
    if (TimeUtils::GetElapsedTime(creationTime, TimeUtils::GetCurrentTime()) > 5) {
        return true;
    } else {
        return false;
    }
}