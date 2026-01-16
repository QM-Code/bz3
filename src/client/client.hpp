#pragma once
#include <cstdint>
#include "engine/types.hpp"
#include <glm/vec3.hpp>
#include "actor.hpp"

#include <string>

class Game;

class Client : public Actor {
private:
    render_id renderId;
    bool justSpawned = false;
    glm::vec3 lastSpawnPosition{0.0f};

    void syncRenderFromState();

public:
    Client(Game &game, client_id id, const PlayerState &initialState);
    ~Client();

    std::string getName() const { return state.name; }
    int getScore() const { return state.score; }

    void update(TimeUtils::duration deltaTime) override;
    void setState(const PlayerState &newState) override;
    void die() override;
    void spawn(glm::vec3 position, glm::quat rotation, glm::vec3 velocity) override;
};
