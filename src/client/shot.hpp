#pragma once
#include "engine/types.hpp"
#include "engine/components/audio.hpp"

class Game;

class Shot {
private:
    Game &game;
    shot_id id;
    bool isGlobalId;
    glm::vec3 position;
    glm::vec3 velocity;

    render_id renderId;
    Audio& audioEngine;
    AudioClip fireAudio;
    AudioClip ricochetAudio;

    shot_id getNextLocalShotId() {
        static shot_id nextId = 1;
        return nextId++;
    };

    Shot(Game &game, shot_id id, bool isGlobalId, glm::vec3 position, glm::vec3 velocity);

public:
    // Local Id shots
    Shot(Game &game, glm::vec3 position, glm::vec3 velocity);
    // Global Id shots
    Shot(Game &game, shot_id globalId, glm::vec3 position, glm::vec3 velocity);
    
    ~Shot();

    void update(TimeUtils::duration deltaTime);
    bool isEqual(shot_id otherId, bool otherIsGlobalId);
};