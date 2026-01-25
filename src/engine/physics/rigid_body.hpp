#pragma once

#include "physics/backend.hpp"
#include <cstdint>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <memory>

class PhysicsRigidBody {
public:
    PhysicsRigidBody() = default;
    explicit PhysicsRigidBody(std::unique_ptr<physics_backend::PhysicsRigidBodyBackend> backend);
    PhysicsRigidBody(const PhysicsRigidBody&) = delete;
    PhysicsRigidBody& operator=(const PhysicsRigidBody&) = delete;
    PhysicsRigidBody(PhysicsRigidBody&& other) noexcept = default;
    PhysicsRigidBody& operator=(PhysicsRigidBody&& other) noexcept = default;
    ~PhysicsRigidBody();

    bool isValid() const;

    glm::vec3 getPosition() const;
    glm::quat getRotation() const;
    glm::vec3 getVelocity() const;
    glm::vec3 getAngularVelocity() const;
    glm::vec3 getForwardVector() const;

    void setPosition(const glm::vec3& position);
    void setRotation(const glm::quat& rotation);
    void setVelocity(const glm::vec3& velocity);
    void setAngularVelocity(const glm::vec3& angularVelocity);

    bool isGrounded(const glm::vec3& dimensions) const;

    void destroy();

    std::uintptr_t nativeHandle() const;

private:
    std::unique_ptr<physics_backend::PhysicsRigidBodyBackend> backend_;
};
