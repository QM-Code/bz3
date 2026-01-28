#pragma once

#include "physics/backend.hpp"
#include <PxPhysicsAPI.h>

namespace physics_backend {

class PhysicsWorldPhysX;

class PhysicsPlayerControllerPhysX final : public PhysicsPlayerControllerBackend {
public:
    PhysicsPlayerControllerPhysX(PhysicsWorldPhysX* world,
                                 const glm::vec3& halfExtents,
                                 const glm::vec3& startPosition);
    ~PhysicsPlayerControllerPhysX() override;

    glm::vec3 getPosition() const override;
    glm::quat getRotation() const override;
    glm::vec3 getVelocity() const override;
    glm::vec3 getAngularVelocity() const override;
    glm::vec3 getForwardVector() const override;
    void setHalfExtents(const glm::vec3& extents) override;
    void update(float dt) override;
    void setPosition(const glm::vec3& position) override;
    void setRotation(const glm::quat& rotation) override;
    void setVelocity(const glm::vec3& velocity) override;
    void setAngularVelocity(const glm::vec3& angularVelocity) override;
    bool isGrounded() const override;
    void destroy() override;

private:
    PhysicsWorldPhysX* world_ = nullptr;
    glm::vec3 halfExtents{0.5f, 1.0f, 0.5f};
    glm::vec3 position{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 velocity{0.0f};
    glm::vec3 angularVelocity{0.0f};
    bool groundedState = false;
    bool lastReportedGrounded = false;
    float logTimer = 0.0f;
    int groundScore = 0;
    static constexpr int kGroundScoreMax = 3;
    static constexpr int kGroundScoreThreshold = 2;
    float stepOffset = 0.2f;
    float slopeLimitRadians = 0.8726646f;
    float gravity = -9.8f;
    physx::PxController* controller_ = nullptr;
};

} // namespace physics_backend
