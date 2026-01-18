#pragma once
#include <cstdint>
#include <string>
#include "engine/types.hpp"
#include <glm/vec3.hpp>

class Game;

class Actor {
protected:
    Game &game;
    client_id id;
    PlayerState state;

public:
    Actor(Game &game, client_id id) : game(game), id(id) {}
    bool isEqual(client_id otherId) const;
    const PlayerState &getState() const;

    void setLocation(const glm::vec3 &position, const glm::quat &rotation, const glm::vec3 &velocity);
    void setScore(int score);

    float getParameter(const std::string &paramName, float defaultValue = 0.0f) const;
    void setParameters(const PlayerParameters &params);
    void setParameters(PlayerParameters &&params);
    virtual void die();

    virtual ~Actor();
    virtual void update(TimeUtils::duration deltaTime) = 0;
    virtual void setState(const PlayerState &state) = 0;
    virtual void spawn(glm::vec3 position, glm::quat rotation, glm::vec3 velocity) = 0;
};
