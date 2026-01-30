#include "physics/backends/bullet/player_controller_bullet.hpp"
#include "physics/backends/bullet/physics_world_bullet.hpp"
#include <btBulletDynamicsCommon.h>
#include <BulletCollision/CollisionDispatch/btGhostObject.h>
#include <BulletDynamics/Character/btKinematicCharacterController.h>
#include <glm/gtc/quaternion.hpp>
#include <cmath>

namespace {
inline btVector3 toBt(const glm::vec3& v) { return btVector3(v.x, v.y, v.z); }
inline glm::vec3 toGlm(const btVector3& v) { return glm::vec3(v.x(), v.y(), v.z()); }

inline float capsuleHalfHeightFromExtents(const glm::vec3& halfExtents) {
    const float radius = std::max(halfExtents.x, halfExtents.z);
    const float height = std::max(0.0f, 2.0f * halfExtents.y - 2.0f * radius);
    return radius + 0.5f * height;
}
}

namespace physics_backend {

PhysicsPlayerControllerBullet::PhysicsPlayerControllerBullet(PhysicsWorldBullet* world,
                                                             const glm::vec3& halfExtents,
                                                             const glm::vec3& startPosition)
    : world_(world), halfExtents(halfExtents) {
    capsuleHalfHeight = capsuleHalfHeightFromExtents(halfExtents);
    rebuildController(startPosition + glm::vec3(0.0f, capsuleHalfHeight, 0.0f));
}

PhysicsPlayerControllerBullet::~PhysicsPlayerControllerBullet() {
    destroy();
}

void PhysicsPlayerControllerBullet::rebuildController(const glm::vec3& centerPosition) {
    if (!world_ || !world_->world()) return;

    if (controller_) {
        world_->world()->removeAction(controller_.get());
    }
    if (ghost_) {
        world_->world()->removeCollisionObject(ghost_.get());
    }

    const float radius = std::max(halfExtents.x, halfExtents.z);
    const float height = std::max(0.0f, 2.0f * halfExtents.y - 2.0f * radius);
    capsuleHalfHeight = radius + 0.5f * height;
    shape_ = std::make_unique<btCapsuleShape>(radius, height);
    ghost_ = std::make_unique<btPairCachingGhostObject>();

    btTransform transform;
    transform.setIdentity();
    transform.setOrigin(toBt(centerPosition));
    ghost_->setWorldTransform(transform);
    ghost_->setCollisionShape(shape_.get());
    ghost_->setCollisionFlags(btCollisionObject::CF_CHARACTER_OBJECT);

    controller_ = std::make_unique<btKinematicCharacterController>(ghost_.get(), shape_.get(), stepHeight);
    controller_->setUseGhostSweepTest(true);
    controller_->setMaxSlope(btScalar(50.0f * SIMD_PI / 180.0f));
    controller_->setGravity(btVector3(0.0f, -gravityMagnitude, 0.0f));

    world_->world()->addCollisionObject(ghost_.get(),
                                        btBroadphaseProxy::CharacterFilter,
                                        btBroadphaseProxy::StaticFilter | btBroadphaseProxy::DefaultFilter);
    world_->world()->addAction(controller_.get());
    lastPosition = getPosition();
}

glm::vec3 PhysicsPlayerControllerBullet::getPosition() const {
    if (!ghost_) return glm::vec3(0.0f);
    btTransform transform = ghost_->getWorldTransform();
    btVector3 pos = transform.getOrigin();
    return glm::vec3(pos.x(), pos.y(), pos.z()) - glm::vec3(0.0f, capsuleHalfHeight, 0.0f);
}

glm::quat PhysicsPlayerControllerBullet::getRotation() const { return rotation; }
glm::vec3 PhysicsPlayerControllerBullet::getVelocity() const { return velocity; }
glm::vec3 PhysicsPlayerControllerBullet::getAngularVelocity() const { return angularVelocity; }

glm::vec3 PhysicsPlayerControllerBullet::getForwardVector() const {
    return rotation * glm::vec3(0, 0, -1);
}

void PhysicsPlayerControllerBullet::setHalfExtents(const glm::vec3& extents) {
    halfExtents = extents;
    const glm::vec3 currentPos = getPosition();
    rebuildController(currentPos + glm::vec3(0.0f, capsuleHalfHeight, 0.0f));
}

bool PhysicsPlayerControllerBullet::probeGround(float probeDepth, glm::vec3* outNormal) const {
    if (!world_ || !world_->world() || !ghost_ || !shape_) return false;

    btTransform startTransform = ghost_->getWorldTransform();
    btTransform endTransform = startTransform;
    endTransform.setOrigin(startTransform.getOrigin() + btVector3(0.0f, -probeDepth, 0.0f));

    struct GroundCallback final : btCollisionWorld::ClosestConvexResultCallback {
        const btCollisionObject* ignore = nullptr;
        GroundCallback(const btVector3& from, const btVector3& to, const btCollisionObject* ignoreIn)
            : btCollisionWorld::ClosestConvexResultCallback(from, to), ignore(ignoreIn) {}

        bool needsCollision(btBroadphaseProxy* proxy0) const override {
            if (proxy0->m_clientObject == ignore) {
                return false;
            }
            return btCollisionWorld::ClosestConvexResultCallback::needsCollision(proxy0);
        }
    };

    GroundCallback callback(startTransform.getOrigin(), endTransform.getOrigin(), ghost_.get());
    callback.m_collisionFilterGroup = btBroadphaseProxy::CharacterFilter;
    callback.m_collisionFilterMask = btBroadphaseProxy::StaticFilter | btBroadphaseProxy::DefaultFilter;
    world_->world()->convexSweepTest(shape_.get(), startTransform, endTransform, callback);
    if (!callback.hasHit()) return false;

    const btVector3 n = callback.m_hitNormalWorld;
    if (outNormal) {
        *outNormal = glm::vec3(n.x(), n.y(), n.z());
    }
    return n.dot(btVector3(0, 1, 0)) > 0.7f;
}

void PhysicsPlayerControllerBullet::update(float dt) {
    if (!controller_ || dt <= 0.0f) return;

    if (world_ && world_->world()) {
        const btVector3 g = world_->world()->getGravity();
        gravityMagnitude = std::abs(static_cast<float>(g.y()));
        controller_->setGravity(btVector3(0.0f, -gravityMagnitude, 0.0f));
    }

    bool jumpingNow = desiredVelocity.y > 1.0f;
    if (jumpingNow && !wasJumping) {
        jumpQueued = true;
    }
    wasJumping = jumpingNow;

    if (jumpQueued && controller_ && controller_->canJump()) {
        controller_->setJumpSpeed(desiredVelocity.y);
        controller_->jump();
        desiredVelocity.y = 0.0f;
        jumpQueued = false;
    }

    velocity.x = desiredVelocity.x;
    velocity.z = desiredVelocity.z;

    const float probeDepth = capsuleHalfHeight + 0.15f;
    glm::vec3 groundNormal(0.0f, 1.0f, 0.0f);
    bool groundedProbe = probeGround(probeDepth, &groundNormal);
    if (groundedProbe) {
        lastGroundNormal = groundNormal;
    }

    glm::vec3 walkDirGlm(velocity.x, 0.0f, velocity.z);
    if (groundedProbe) {
        const glm::vec3 n = glm::normalize(lastGroundNormal);
        walkDirGlm -= n * glm::dot(walkDirGlm, n);
    }
    btVector3 walkDir(walkDirGlm.x, 0.0f, walkDirGlm.z);
    controller_->setWalkDirection(walkDir * btScalar(dt));

    const glm::vec3 currentPos = getPosition();
    const glm::vec3 actualVelocity = (currentPos - lastPosition) / dt;
    lastPosition = currentPos;

    const bool movingUp = actualVelocity.y > 0.1f;
    if (movingUp) {
        groundScore = 0;
        groundedState = false;
    }
    const bool onGroundNow = controller_ ? controller_->onGround() : false;
    const bool groundHit = (onGroundNow || groundedProbe) && !movingUp;
    if (groundHit) {
        groundScore = std::min(groundScore + 1, kGroundScoreMax);
    } else {
        groundScore = std::max(groundScore - 1, 0);
    }
    groundedState = groundScore >= kGroundScoreThreshold;
    velocity = actualVelocity;
    if (groundedState && velocity.y < 0.0f) {
        velocity.y = 0.0f;
    }

    if (glm::dot(angularVelocity, angularVelocity) > 0.f) {
        glm::quat dq = glm::quat(0, angularVelocity.x, angularVelocity.y, angularVelocity.z) * rotation;
        rotation = glm::normalize(rotation + 0.5f * dq * dt);
    }
}

void PhysicsPlayerControllerBullet::setPosition(const glm::vec3& position) {
    if (!ghost_) return;
    btTransform transform = ghost_->getWorldTransform();
    transform.setOrigin(toBt(position + glm::vec3(0.0f, capsuleHalfHeight, 0.0f)));
    ghost_->setWorldTransform(transform);
    if (controller_ && world_ && world_->world()) {
        controller_->reset(world_->world());
    }
    lastPosition = position;
}

void PhysicsPlayerControllerBullet::setRotation(const glm::quat& rotationIn) {
    rotation = glm::normalize(rotationIn);
}

void PhysicsPlayerControllerBullet::setVelocity(const glm::vec3& velocity) {
    desiredVelocity = velocity;
    this->velocity.x = velocity.x;
    this->velocity.z = velocity.z;
}

void PhysicsPlayerControllerBullet::setAngularVelocity(const glm::vec3& angularVelocityIn) {
    angularVelocity = angularVelocityIn;
}

bool PhysicsPlayerControllerBullet::isGrounded() const {
    return controller_ ? groundedState : false;
}

void PhysicsPlayerControllerBullet::destroy() {
    if (world_ && world_->world()) {
        if (controller_) {
            world_->world()->removeAction(controller_.get());
        }
        if (ghost_) {
            world_->world()->removeCollisionObject(ghost_.get());
        }
    }
    controller_.reset();
    ghost_.reset();
    shape_.reset();
    world_ = nullptr;
}

} // namespace physics_backend
