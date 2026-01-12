#include "engine/physics/rigid_body.hpp"
#include "engine/physics/physics_world.hpp"
#include <bullet/btBulletDynamicsCommon.h>
#include <spdlog/spdlog.h>

PhysicsRigidBody::PhysicsRigidBody(PhysicsWorld* world, btRigidBody* body)
    : world_(world), body_(body) {}

PhysicsRigidBody::PhysicsRigidBody(PhysicsRigidBody&& other) noexcept {
    world_ = other.world_;
    body_ = other.body_;
    other.world_ = nullptr;
    other.body_ = nullptr;
}

PhysicsRigidBody& PhysicsRigidBody::operator=(PhysicsRigidBody&& other) noexcept {
    if (this == &other) {
        return *this;
    }

    destroy();

    world_ = other.world_;
    body_ = other.body_;
    other.world_ = nullptr;
    other.body_ = nullptr;
    return *this;
}

PhysicsRigidBody::~PhysicsRigidBody() {
    destroy();
}

void PhysicsRigidBody::destroy() {
    if (!isValid()) {
        return;
    }

    world_->removeBody(body_);
    body_ = nullptr;
    world_ = nullptr;
}

void PhysicsRigidBody::ensureValid(const char* caller) const {
    if (!isValid()) {
        spdlog::error("{}: Attempted to use an invalid rigid body", caller);
    }
}

glm::vec3 PhysicsRigidBody::getPosition() const {
    ensureValid("PhysicsRigidBody::getPosition");
    if (!isValid()) {
        return glm::vec3(0.0f);
    }

    btTransform transform;
    body_->getMotionState()->getWorldTransform(transform);
    btVector3 pos = transform.getOrigin();
    return glm::vec3(pos.x(), pos.y(), pos.z());
}

glm::quat PhysicsRigidBody::getRotation() const {
    ensureValid("PhysicsRigidBody::getRotation");
    if (!isValid()) {
        return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    }

    btTransform transform;
    body_->getMotionState()->getWorldTransform(transform);
    btQuaternion rot = transform.getRotation();
    return glm::quat(rot.w(), rot.x(), rot.y(), rot.z());
}

glm::vec3 PhysicsRigidBody::getVelocity() const {
    ensureValid("PhysicsRigidBody::getVelocity");
    if (!isValid()) {
        return glm::vec3(0.0f);
    }

    btVector3 vel = body_->getLinearVelocity();
    return glm::vec3(vel.x(), vel.y(), vel.z());
}

glm::vec3 PhysicsRigidBody::getAngularVelocity() const {
    ensureValid("PhysicsRigidBody::getAngularVelocity");
    if (!isValid()) {
        return glm::vec3(0.0f);
    }

    btVector3 vel = body_->getAngularVelocity();
    return glm::vec3(vel.x(), vel.y(), vel.z());
}

glm::vec3 PhysicsRigidBody::getForwardVector() const {
    ensureValid("PhysicsRigidBody::getForwardVector");
    if (!isValid()) {
        return glm::vec3(0.0f, 0.0f, -1.0f);
    }

    btTransform transform;
    body_->getMotionState()->getWorldTransform(transform);
    btMatrix3x3 rot = transform.getBasis();
    btVector3 forward = rot * btVector3(0, 0, -1);
    glm::vec3 result(forward.x(), forward.y(), forward.z());
    return glm::normalize(result);
}

void PhysicsRigidBody::setPosition(const glm::vec3& position) {
    ensureValid("PhysicsRigidBody::setPosition");
    if (!isValid()) {
        return;
    }

    btTransform transform;
    body_->getMotionState()->getWorldTransform(transform);
    transform.setOrigin(btVector3(position.x, position.y, position.z));
    body_->getMotionState()->setWorldTransform(transform);
    body_->setWorldTransform(transform);
    body_->activate(true);
}

void PhysicsRigidBody::setRotation(const glm::quat& rotation) {
    ensureValid("PhysicsRigidBody::setRotation");
    if (!isValid()) {
        return;
    }

    btTransform transform;
    body_->getMotionState()->getWorldTransform(transform);
    transform.setRotation(btQuaternion(rotation.x, rotation.y, rotation.z, rotation.w));
    body_->getMotionState()->setWorldTransform(transform);
    body_->setWorldTransform(transform);
    body_->activate(true);
}

void PhysicsRigidBody::setVelocity(const glm::vec3& velocity) {
    ensureValid("PhysicsRigidBody::setVelocity");
    if (!isValid()) {
        return;
    }

    body_->setLinearVelocity(btVector3(velocity.x, velocity.y, velocity.z));
    body_->activate(true);
}

void PhysicsRigidBody::setAngularVelocity(const glm::vec3& angularVelocity) {
    ensureValid("PhysicsRigidBody::setAngularVelocity");
    if (!isValid()) {
        return;
    }

    body_->setAngularVelocity(btVector3(angularVelocity.x, angularVelocity.y, angularVelocity.z));
    body_->activate(true);
}

bool PhysicsRigidBody::isGrounded(const glm::vec3& dimensions) const {
    ensureValid("PhysicsRigidBody::isGrounded");
    if (!isValid()) {
        return false;
    }

    btVector3 halfExtents(dimensions.x * 0.5f, dimensions.y * 0.5f, dimensions.z * 0.5f);
    btBoxShape boxShape(halfExtents);

    btTransform from = body_->getWorldTransform();
    btTransform to = from;
    to.setOrigin(from.getOrigin() - btVector3(0, 0.1f, 0));

    btCollisionWorld::ClosestConvexResultCallback cb(from.getOrigin(), to.getOrigin());
    cb.m_collisionFilterGroup = body_->getBroadphaseHandle()->m_collisionFilterGroup;
    cb.m_collisionFilterMask = body_->getBroadphaseHandle()->m_collisionFilterMask;

    world_->world_->convexSweepTest(&boxShape, from, to, cb);

    return cb.hasHit() && cb.m_hitNormalWorld.dot(btVector3(0, 1, 0)) > 0.7f;
}
