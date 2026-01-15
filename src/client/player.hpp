#pragma once
#include <glm/glm.hpp>
#include <string>
#include "engine/types.hpp"
#include "engine/physics/player_controller.hpp"
#include "engine/components/audio.hpp"
#include <spdlog/spdlog.h>

#define POSITION_UPDATE_THRESHOLD 0.01f
#define ROTATION_UPDATE_THRESHOLD 0.01f

class Game;

class Player {
private:
    Game &game;
    client_id clientId;
    bool grounded;

    PhysicsPlayerController* physics = nullptr;
    Audio& audioEngine;
    AudioClip jumpAudio;
    AudioClip dieAudio;
    AudioClip spawnAudio;
    AudioClip landAudio;

    TimeUtils::time lastJumpTime;
    TimeUtils::duration jumpCooldown;

    glm::vec3 lastPosition;
    glm::quat lastRotation;
    PlayerState state;

    float getParameter(const std::string &paramName) const;

public:
    Player(Game &game, client_id, PlayerParameters params, const std::string name);
    ~Player();

    std::string getName() const { return state.name; }

    client_id getClientId() const { return clientId; }

    glm::vec3 getPosition() const { return state.position; }
    glm::vec3 getVelocity() const { return state.velocity; }
    glm::quat getRotation() const { return state.rotation; }
    glm::vec3 getForwardVector() const;
    int getScore() const { return state.score; }
    void setScore(int score) { state.score = score; }

    void setExtents(const glm::vec3& extents);

    void handleParameters(const ServerMsg_PlayerParameters &msg);
    void handleDeath();
    void handleSpawn(const ServerMsg_PlayerSpawn &msg);
    void earlyUpdate();
    void lateUpdate();
};
