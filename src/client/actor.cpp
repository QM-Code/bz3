#include "actor.hpp"
#include "game.hpp"

#include <utility>

#include <spdlog/spdlog.h>

Actor::~Actor() = default;

void Actor::setLocation(const glm::vec3 &position, const glm::quat &rotation, const glm::vec3 &velocity) {
    state.position = position;
    state.rotation = rotation;
    state.velocity = velocity;
}

void Actor::setScore(int score) {
    state.score = score;
}

float Actor::getParameter(const std::string &paramName, float defaultValue) const {
    auto it = state.params.find(paramName);
    if (it != state.params.end()) {
        return it->second;
    }
    spdlog::warn("Actor::getParameter: Parameter '{}' not found, returning {}", paramName, defaultValue);
    return defaultValue;
}

void Actor::setParameters(const PlayerParameters &params) {
    for (const auto &pair : params) {
        state.params[pair.first] = pair.second;
    }
}

void Actor::setParameters(PlayerParameters &&params) {
    state.params = std::move(params);
}

void Actor::die() {
    auto fx = game.engine.particles->createEffect(game.world->getAssetPath("effects.explosion").string(), 1.0f);
    fx.setPosition(state.position);
}

bool Actor::isEqual(client_id otherId) const {
    return id == otherId;
}

const PlayerState &Actor::getState() const {
    return state;
}