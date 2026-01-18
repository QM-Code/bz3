#pragma once

#include <Jolt/Jolt.h>
#include "engine/physics/rigid_body.hpp"
#include "engine/physics/static_body.hpp"
#include <Jolt/Physics/Body/BodyID.h>
#include <memory>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>

namespace JPH {
class PhysicsSystem;
class JobSystem;
class TempAllocator;
}

class PhysicsPlayerController;

namespace PhysicsLayers {
using ObjectLayer = uint8_t;
inline constexpr ObjectLayer NonMoving = 0;
inline constexpr ObjectLayer Moving = 1;
}

struct PhysicsMaterial {
    float friction = 0.0f;
    float restitution = 0.0f;
    float rollingFriction = 0.0f;
    float spinningFriction = 0.0f;
};

class PhysicsWorld {
public:
    PhysicsWorld();
    ~PhysicsWorld();

    PhysicsWorld(const PhysicsWorld&) = delete;
    PhysicsWorld& operator=(const PhysicsWorld&) = delete;

    void update(float deltaTime);

    void setGravity(float gravity);

    PhysicsRigidBody createBoxBody(const glm::vec3& halfExtents,
                                   float mass,
                                   const glm::vec3& position,
                                   const PhysicsMaterial& material);

    PhysicsPlayerController& createPlayer();
    PhysicsPlayerController& createPlayer(const glm::vec3& size);

    PhysicsPlayerController* playerController() { return playerController_.get(); }

    PhysicsStaticBody createStaticMesh(const std::string& meshPath);

    bool raycast(const glm::vec3& from, const glm::vec3& to, glm::vec3& hitPoint, glm::vec3& hitNormal) const;

private:
    friend class PhysicsRigidBody;
    friend class PhysicsPlayerController;
    friend class PhysicsStaticBody;

    void removeBody(const JPH::BodyID& id) const;

    JPH::PhysicsSystem* physicsSystem() { return physicsSystem_.get(); }
    const JPH::PhysicsSystem* physicsSystem() const { return physicsSystem_.get(); }

    std::unique_ptr<PhysicsPlayerController> playerController_;

    std::unique_ptr<JPH::TempAllocator> tempAllocator_;
    std::unique_ptr<JPH::JobSystem> jobSystem_;
    std::unique_ptr<JPH::PhysicsSystem> physicsSystem_;
};
