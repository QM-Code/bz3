#include "engine/physics/rigid_body.hpp"
#include "engine/physics/physics_world.hpp"
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/ShapeCast.h>
#include <Jolt/Physics/PhysicsSystem.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

using namespace JPH;

namespace {
template <class TVec>
inline glm::vec3 toGlm(const TVec& v) { return glm::vec3(static_cast<float>(v.GetX()), static_cast<float>(v.GetY()), static_cast<float>(v.GetZ())); }
}

PhysicsRigidBody::PhysicsRigidBody(PhysicsWorld* world, const BodyID& bodyId)
    : world_(world), body_(bodyId) {}

PhysicsRigidBody::PhysicsRigidBody(PhysicsRigidBody&& other) noexcept {
    world_ = other.world_;
    body_ = other.body_;
    other.world_ = nullptr;
    other.body_.reset();
}

PhysicsRigidBody& PhysicsRigidBody::operator=(PhysicsRigidBody&& other) noexcept {
    if (this == &other) {
        return *this;
    }

    destroy();

    world_ = other.world_;
    body_ = other.body_;
    other.world_ = nullptr;
    other.body_.reset();
    return *this;
}

PhysicsRigidBody::~PhysicsRigidBody() {
    destroy();
}

void PhysicsRigidBody::destroy() {
    if (!world_ || !body_.has_value()) {
        return;
    }

    world_->removeBody(*body_);
    body_.reset();
    world_ = nullptr;
}

glm::vec3 PhysicsRigidBody::getPosition() const {
    const BodyInterface& bi = world_->physicsSystem()->GetBodyInterface();
    RVec3 pos = bi.GetCenterOfMassPosition(*body_);
    return toGlm(pos);
}

glm::quat PhysicsRigidBody::getRotation() const {
    const BodyInterface& bi = world_->physicsSystem()->GetBodyInterface();
    Quat rot = bi.GetRotation(*body_);
    return glm::quat(rot.GetW(), rot.GetX(), rot.GetY(), rot.GetZ());
}

glm::vec3 PhysicsRigidBody::getVelocity() const {
    const BodyInterface& bi = world_->physicsSystem()->GetBodyInterface();
    Vec3 vel = bi.GetLinearVelocity(*body_);
    return toGlm(vel);
}

glm::vec3 PhysicsRigidBody::getAngularVelocity() const {
    const BodyInterface& bi = world_->physicsSystem()->GetBodyInterface();
    Vec3 vel = bi.GetAngularVelocity(*body_);
    return toGlm(vel);
}

glm::vec3 PhysicsRigidBody::getForwardVector() const {
    const BodyInterface& bi = world_->physicsSystem()->GetBodyInterface();
    Quat rot = bi.GetRotation(*body_);
    glm::vec3 forward = glm::rotate(glm::quat(rot.GetW(), rot.GetX(), rot.GetY(), rot.GetZ()), glm::vec3(0, 0, -1));
    return glm::normalize(forward);
}

void PhysicsRigidBody::setPosition(const glm::vec3& position) {
    BodyInterface& bi = world_->physicsSystem()->GetBodyInterface();
    bi.SetPosition(*body_, RVec3(position.x, position.y, position.z), EActivation::Activate);
}

void PhysicsRigidBody::setRotation(const glm::quat& rotation) {
    BodyInterface& bi = world_->physicsSystem()->GetBodyInterface();
    bi.SetRotation(*body_, Quat(rotation.x, rotation.y, rotation.z, rotation.w), EActivation::Activate);
}

void PhysicsRigidBody::setVelocity(const glm::vec3& velocity) {
    BodyInterface& bi = world_->physicsSystem()->GetBodyInterface();
    bi.SetLinearVelocity(*body_, Vec3(velocity.x, velocity.y, velocity.z));
    bi.ActivateBody(*body_);
}

void PhysicsRigidBody::setAngularVelocity(const glm::vec3& angularVelocity) {
    BodyInterface& bi = world_->physicsSystem()->GetBodyInterface();
    bi.SetAngularVelocity(*body_, Vec3(angularVelocity.x, angularVelocity.y, angularVelocity.z));
    bi.ActivateBody(*body_);
}

bool PhysicsRigidBody::isGrounded(const glm::vec3& dimensions) const {
    if (!world_ || !body_.has_value()) return false;

    RefConst<Shape> shape = new BoxShape(Vec3(dimensions.x * 0.5f, dimensions.y * 0.5f, dimensions.z * 0.5f));
    RMat44 transform = world_->physicsSystem()->GetBodyInterface().GetCenterOfMassTransform(*body_);
    RShapeCast shapeCast(shape, Vec3::sReplicate(1.0f), transform, Vec3(0, -0.1f, 0));

    ShapeCastSettings settings;
    ClosestHitCollisionCollector<CastShapeCollector> collector;
    world_->physicsSystem()->GetNarrowPhaseQuery().CastShape(shapeCast, settings, RVec3::sZero(), collector);
    if (!collector.HadHit()) return false;

    Vec3 n = collector.mHit.mPenetrationAxis;
    if (n.LengthSq() < 1e-6f) return false;
    n = n.Normalized();
    return n.Dot(Vec3(0, 1, 0)) > 0.7f;
}

JPH::BodyID PhysicsRigidBody::nativeHandle() const {
    return body_.value_or(JPH::BodyID());
}
