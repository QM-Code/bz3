#pragma once

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <memory>
class PhysicsWorld;

class PhysicsPlayerController {
public:
    PhysicsPlayerController(PhysicsWorld* world, const glm::vec3& halfExtents, const glm::vec3& startPosition = glm::vec3(0.0f));
    PhysicsPlayerController(const PhysicsPlayerController&) = delete;
    PhysicsPlayerController& operator=(const PhysicsPlayerController&) = delete;
    PhysicsPlayerController(PhysicsPlayerController&& other) noexcept;
    PhysicsPlayerController& operator=(PhysicsPlayerController&& other) noexcept;
    ~PhysicsPlayerController();

    glm::vec3 getPosition() const;
    glm::quat getRotation() const;
    glm::vec3 getVelocity() const;
    glm::vec3 getAngularVelocity() const;
    glm::vec3 getForwardVector() const;
	void setHalfExtents(const glm::vec3& extents);

    // Advance the controller by dt using Jolt's CharacterVirtual helper.
    void update(float dt);

    void setPosition(const glm::vec3& position);
    void setRotation(const glm::quat& rotation);
    void setVelocity(const glm::vec3& velocity);
    void setAngularVelocity(const glm::vec3& angularVelocity);

    bool isGrounded() const;

    void destroy();

private:
    PhysicsWorld* world = nullptr;
    JPH::Ref<JPH::CharacterVirtual> character_;
    glm::vec3 halfExtents{0.5f, 1.0f, 0.5f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 velocity{0.0f};
    glm::vec3 angularVelocity{0.0f};
    float gravity = -9.8f; // downward acceleration (m/s^2)
    float characterPadding = 0.05f; // padding Jolt adds around the shape
    float groundSupportBand = 0.1f; // vertical band above the feet that counts as supporting ground

    glm::vec3 centerPosition() const;
};
