#include "physics/backends/physx/player_controller_physx.hpp"
#include "physics/backends/physx/physics_world_physx.hpp"
#include <PxPhysicsAPI.h>
#include <characterkinematic/PxController.h>
#include <characterkinematic/PxControllerManager.h>
#include <algorithm>
#include <spdlog/spdlog.h>

namespace physics_backend {

PhysicsPlayerControllerPhysX::PhysicsPlayerControllerPhysX(PhysicsWorldPhysX* world,
                                                           const glm::vec3& halfExtentsIn,
                                                           const glm::vec3& startPosition)
    : world_(world), halfExtents(halfExtentsIn), position(startPosition) {
    if (!world_ || !world_->physics() || !world_->scene() || !world_->controllerManager()) {
        return;
    }

    physx::PxCapsuleControllerDesc desc;
    desc.position = physx::PxExtendedVec3(startPosition.x, startPosition.y + halfExtents.y, startPosition.z);
    desc.radius = std::max(halfExtents.x, halfExtents.z);
    desc.height = std::max(0.0f, 2.0f * halfExtents.y - 2.0f * desc.radius);
    desc.stepOffset = stepOffset;
    desc.slopeLimit = std::cos(slopeLimitRadians);
    desc.contactOffset = 0.1f;
    desc.material = world_->defaultMaterial();
    desc.upDirection = physx::PxVec3(0.0f, 1.0f, 0.0f);
    desc.reportCallback = nullptr;
    desc.behaviorCallback = nullptr;
    desc.nonWalkableMode = physx::PxControllerNonWalkableMode::ePREVENT_CLIMBING;

    controller_ = world_->controllerManager()->createController(desc);
    if (controller_) {
        controller_->setFootPosition(physx::PxExtendedVec3(startPosition.x, startPosition.y, startPosition.z));
        if (auto* actor = controller_->getActor()) {
            physx::PxShape* shapes[8];
            const physx::PxU32 count = actor->getShapes(shapes, 8);
            for (physx::PxU32 i = 0; i < count; ++i) {
                if (shapes[i]) {
                    shapes[i]->setQueryFilterData(physx::PxFilterData(kPhysXQueryIgnorePlayer, 0, 0, 0));
                }
            }
        }
    }
}

PhysicsPlayerControllerPhysX::~PhysicsPlayerControllerPhysX() {
    destroy();
}

glm::vec3 PhysicsPlayerControllerPhysX::getPosition() const { return position; }
glm::quat PhysicsPlayerControllerPhysX::getRotation() const { return rotation; }
glm::vec3 PhysicsPlayerControllerPhysX::getVelocity() const { return velocity; }
glm::vec3 PhysicsPlayerControllerPhysX::getAngularVelocity() const { return angularVelocity; }
glm::vec3 PhysicsPlayerControllerPhysX::getForwardVector() const { return rotation * glm::vec3(0, 0, -1); }

void PhysicsPlayerControllerPhysX::setHalfExtents(const glm::vec3& extents) {
    halfExtents = extents;
}

void PhysicsPlayerControllerPhysX::update(float dt) {
    if (!controller_ || dt <= 0.0f) return;

    logTimer += dt;

    const bool grounded = groundedState;
    if (!grounded) {
        velocity.y += gravity * dt;
    } else if (velocity.y < 0.0f) {
        velocity.y = 0.0f;
    }

    physx::PxVec3 disp(velocity.x * dt, velocity.y * dt, velocity.z * dt);
    physx::PxControllerFilters filters;
    physx::PxControllerCollisionFlags flags = controller_->move(disp, 0.0f, dt, filters);
    const physx::PxExtendedVec3 foot = controller_->getFootPosition();
    position = glm::vec3(static_cast<float>(foot.x), static_cast<float>(foot.y), static_cast<float>(foot.z));

    const bool collisionDown = flags.isSet(physx::PxControllerCollisionFlag::eCOLLISION_DOWN);
    bool rayGrounded = false;
    if (world_) {
        constexpr float probeUp = 0.1f;
        const float probeDown = std::max(0.15f, stepOffset + 0.05f);
        glm::vec3 hitPoint{};
        glm::vec3 hitNormal{};
        const glm::vec3 rayStart = position + glm::vec3(0.0f, probeUp, 0.0f);
        const glm::vec3 rayEnd = position - glm::vec3(0.0f, probeDown, 0.0f);
        rayGrounded = world_->raycast(rayStart, rayEnd, hitPoint, hitNormal);
    }
    const bool movingUp = velocity.y > 0.1f;
    // Sketch for a future jump grace timer (not implemented):
    //   - Add a member float jumpGraceTimer = 0.0f;
    //   - When jump impulse is applied (velocity.y set to jumpSpeed), set jumpGraceTimer = 0.1f.
    //   - Each update: if (jumpGraceTimer > 0.0f) jumpGraceTimer -= dt;
    //   - When jumpGraceTimer > 0.0f, ignore ground hits (treat groundHit as false).
    if (movingUp) {
        groundScore = 0;
        groundedState = false;
    }
    const bool groundHit = (collisionDown || rayGrounded) && !movingUp;
    if (groundHit) {
        groundScore = std::min(groundScore + 1, kGroundScoreMax);
    } else {
        groundScore = std::max(groundScore - 1, 0);
    }
    groundedState = groundScore >= kGroundScoreThreshold;

    if (groundedState && velocity.y < 0.0f) {
        velocity.y = 0.0f;
    }

    if (glm::dot(angularVelocity, angularVelocity) > 0.f) {
        glm::quat dq = glm::quat(0, angularVelocity.x, angularVelocity.y, angularVelocity.z) * rotation;
        rotation = glm::normalize(rotation + 0.5f * dq * dt);
    }

    (void)flags;
    (void)rayGrounded;
}

void PhysicsPlayerControllerPhysX::setPosition(const glm::vec3& positionIn) {
    position = positionIn;
    if (controller_) {
        controller_->setFootPosition(physx::PxExtendedVec3(position.x, position.y, position.z));
    }
}

void PhysicsPlayerControllerPhysX::setRotation(const glm::quat& rotationIn) {
    rotation = glm::normalize(rotationIn);
}

void PhysicsPlayerControllerPhysX::setVelocity(const glm::vec3& velocityIn) {
    velocity = velocityIn;
}

void PhysicsPlayerControllerPhysX::setAngularVelocity(const glm::vec3& angularVelocityIn) {
    angularVelocity = angularVelocityIn;
}

bool PhysicsPlayerControllerPhysX::isGrounded() const {
    return groundedState;
}

void PhysicsPlayerControllerPhysX::destroy() {
    if (controller_) {
        controller_->release();
        controller_ = nullptr;
    }
    world_ = nullptr;
}

} // namespace physics_backend
