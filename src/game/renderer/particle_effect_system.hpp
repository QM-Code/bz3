#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/mat4x4.hpp>

#include <memory>
#include <string>

class ParticleEffect {
public:
    ParticleEffect() = delete;
    ParticleEffect(const ParticleEffect&) = default;
    ParticleEffect(ParticleEffect&&) noexcept = default;
    ParticleEffect& operator=(const ParticleEffect&) = default;
    ParticleEffect& operator=(ParticleEffect&&) noexcept = default;
    ~ParticleEffect() = default;

    void setPosition(glm::vec3 position);
    void setRotation(glm::quat rotation);
    void stop();

private:
    explicit ParticleEffect(std::shared_ptr<struct ParticleEffectData> data);

    std::shared_ptr<struct ParticleEffectData> data_;

    friend class ParticleEngine;
};

class ParticleEngine {
    friend class ParticleEffect;

public:
    ParticleEngine();
    ~ParticleEngine();

    ParticleEffect createEffect(const std::string& filepath, float sizeFactor = 1.0f);

    // Call once per frame to advance particle simulation.
    void update(float deltaSeconds);

    // Call after update to render using the active camera matrices.
    void render(const glm::mat4& view,
                const glm::mat4& projection,
                const glm::vec3& cameraPosition,
                const glm::vec3& cameraFront);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
