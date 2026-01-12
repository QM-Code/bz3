#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <memory>

class btRigidBody;

class PhysicsWorld;

class PhysicsRigidBody {
public:
    PhysicsRigidBody() = default;
    PhysicsRigidBody(PhysicsWorld* world, btRigidBody* body);
    PhysicsRigidBody(const PhysicsRigidBody&) = delete;
    PhysicsRigidBody& operator=(const PhysicsRigidBody&) = delete;
    PhysicsRigidBody(PhysicsRigidBody&& other) noexcept;
    PhysicsRigidBody& operator=(PhysicsRigidBody&& other) noexcept;
    ~PhysicsRigidBody();

    bool isValid() const { return world_ != nullptr && body_ != nullptr; }

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

    btRigidBody* nativeHandle() const { return body_; }

private:
    void ensureValid(const char* caller) const;

    PhysicsWorld* world_ = nullptr;
    btRigidBody* body_ = nullptr;
};
