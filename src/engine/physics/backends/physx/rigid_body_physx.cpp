#include "physics/backends/physx/rigid_body_physx.hpp"
#include "physics/backends/physx/physics_world_physx.hpp"
#include <PxPhysicsAPI.h>
#include <glm/gtc/quaternion.hpp>

namespace {
inline glm::vec3 toGlm(const physx::PxVec3& v) { return glm::vec3(v.x, v.y, v.z); }
inline physx::PxVec3 toPx(const glm::vec3& v) { return physx::PxVec3(v.x, v.y, v.z); }
}

namespace physics_backend {

PhysicsRigidBodyPhysX::PhysicsRigidBodyPhysX(PhysicsWorldPhysX* world, physx::PxRigidActor* actor)
    : world_(world), actor_(actor) {}

PhysicsRigidBodyPhysX::~PhysicsRigidBodyPhysX() {
    destroy();
}

bool PhysicsRigidBodyPhysX::isValid() const {
    return world_ != nullptr && actor_ != nullptr;
}

glm::vec3 PhysicsRigidBodyPhysX::getPosition() const {
    if (!actor_) return glm::vec3(0.0f);
    const physx::PxTransform t = actor_->getGlobalPose();
    return toGlm(t.p);
}

glm::quat PhysicsRigidBodyPhysX::getRotation() const {
    if (!actor_) return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    const physx::PxQuat q = actor_->getGlobalPose().q;
    return glm::quat(q.w, q.x, q.y, q.z);
}

glm::vec3 PhysicsRigidBodyPhysX::getVelocity() const {
    if (!actor_) return glm::vec3(0.0f);
    auto* dynamic = actor_->is<physx::PxRigidDynamic>();
    return dynamic ? toGlm(dynamic->getLinearVelocity()) : glm::vec3(0.0f);
}

glm::vec3 PhysicsRigidBodyPhysX::getAngularVelocity() const {
    if (!actor_) return glm::vec3(0.0f);
    auto* dynamic = actor_->is<physx::PxRigidDynamic>();
    return dynamic ? toGlm(dynamic->getAngularVelocity()) : glm::vec3(0.0f);
}

glm::vec3 PhysicsRigidBodyPhysX::getForwardVector() const {
    if (!actor_) return glm::vec3(0.0f, 0.0f, -1.0f);
    const physx::PxQuat q = actor_->getGlobalPose().q;
    glm::quat rot(q.w, q.x, q.y, q.z);
    return glm::normalize(rot * glm::vec3(0, 0, -1));
}

void PhysicsRigidBodyPhysX::setPosition(const glm::vec3& position) {
    if (!actor_) return;
    physx::PxTransform t = actor_->getGlobalPose();
    t.p = toPx(position);
    actor_->setGlobalPose(t);
}

void PhysicsRigidBodyPhysX::setRotation(const glm::quat& rotation) {
    if (!actor_) return;
    physx::PxTransform t = actor_->getGlobalPose();
    t.q = physx::PxQuat(rotation.x, rotation.y, rotation.z, rotation.w);
    actor_->setGlobalPose(t);
}

void PhysicsRigidBodyPhysX::setVelocity(const glm::vec3& velocity) {
    if (!actor_) return;
    if (auto* dynamic = actor_->is<physx::PxRigidDynamic>()) {
        dynamic->setLinearVelocity(toPx(velocity));
    }
}

void PhysicsRigidBodyPhysX::setAngularVelocity(const glm::vec3& angularVelocity) {
    if (!actor_) return;
    if (auto* dynamic = actor_->is<physx::PxRigidDynamic>()) {
        dynamic->setAngularVelocity(toPx(angularVelocity));
    }
}

bool PhysicsRigidBodyPhysX::isGrounded(const glm::vec3& /*dimensions*/) const {
    return false;
}

void PhysicsRigidBodyPhysX::destroy() {
    if (!actor_ || !world_ || !world_->scene()) {
        actor_ = nullptr;
        world_ = nullptr;
        return;
    }
    world_->scene()->removeActor(*actor_);
    actor_->release();
    actor_ = nullptr;
    world_ = nullptr;
}

std::uintptr_t PhysicsRigidBodyPhysX::nativeHandle() const {
    return reinterpret_cast<std::uintptr_t>(actor_);
}

} // namespace physics_backend
