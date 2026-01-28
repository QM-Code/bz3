#pragma once

#include "physics/backend.hpp"
#include <PxPhysicsAPI.h>

namespace physics_backend {

class PhysicsWorldPhysX;

class PhysicsRigidBodyPhysX final : public PhysicsRigidBodyBackend {
public:
    PhysicsRigidBodyPhysX() = default;
    PhysicsRigidBodyPhysX(PhysicsWorldPhysX* world, physx::PxRigidActor* actor);
    ~PhysicsRigidBodyPhysX() override;

    bool isValid() const override;
    glm::vec3 getPosition() const override;
    glm::quat getRotation() const override;
    glm::vec3 getVelocity() const override;
    glm::vec3 getAngularVelocity() const override;
    glm::vec3 getForwardVector() const override;
    void setPosition(const glm::vec3& position) override;
    void setRotation(const glm::quat& rotation) override;
    void setVelocity(const glm::vec3& velocity) override;
    void setAngularVelocity(const glm::vec3& angularVelocity) override;
    bool isGrounded(const glm::vec3& dimensions) const override;
    void destroy() override;
    std::uintptr_t nativeHandle() const override;

private:
    PhysicsWorldPhysX* world_ = nullptr;
    physx::PxRigidActor* actor_ = nullptr;
};

} // namespace physics_backend
