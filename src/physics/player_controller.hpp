#pragma once

#include "physics/backend.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <memory>

class PhysicsPlayerController {
public:
    PhysicsPlayerController() = default;
    explicit PhysicsPlayerController(std::unique_ptr<physics_backend::PhysicsPlayerControllerBackend> backend);
    PhysicsPlayerController(const PhysicsPlayerController&) = delete;
    PhysicsPlayerController& operator=(const PhysicsPlayerController&) = delete;
    PhysicsPlayerController(PhysicsPlayerController&& other) noexcept = default;
    PhysicsPlayerController& operator=(PhysicsPlayerController&& other) noexcept = default;
    ~PhysicsPlayerController();

    glm::vec3 getPosition() const;
    glm::quat getRotation() const;
    glm::vec3 getVelocity() const;
    glm::vec3 getAngularVelocity() const;
    glm::vec3 getForwardVector() const;
    void setHalfExtents(const glm::vec3& extents);

    void update(float dt);

    void setPosition(const glm::vec3& position);
    void setRotation(const glm::quat& rotation);
    void setVelocity(const glm::vec3& velocity);
    void setAngularVelocity(const glm::vec3& angularVelocity);

    bool isGrounded() const;

    void destroy();

private:
    std::unique_ptr<physics_backend::PhysicsPlayerControllerBackend> backend_;
};
