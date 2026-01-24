#include "physics/rigid_body.hpp"
#include "physics/backend.hpp"

PhysicsRigidBody::PhysicsRigidBody(std::unique_ptr<physics_backend::PhysicsRigidBodyBackend> backend)
    : backend_(std::move(backend)) {}

PhysicsRigidBody::~PhysicsRigidBody() {
    destroy();
}

bool PhysicsRigidBody::isValid() const {
    return backend_ && backend_->isValid();
}

glm::vec3 PhysicsRigidBody::getPosition() const {
    return backend_ ? backend_->getPosition() : glm::vec3(0.0f);
}

glm::quat PhysicsRigidBody::getRotation() const {
    return backend_ ? backend_->getRotation() : glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
}

glm::vec3 PhysicsRigidBody::getVelocity() const {
    return backend_ ? backend_->getVelocity() : glm::vec3(0.0f);
}

glm::vec3 PhysicsRigidBody::getAngularVelocity() const {
    return backend_ ? backend_->getAngularVelocity() : glm::vec3(0.0f);
}

glm::vec3 PhysicsRigidBody::getForwardVector() const {
    return backend_ ? backend_->getForwardVector() : glm::vec3(0.0f, 0.0f, -1.0f);
}

void PhysicsRigidBody::setPosition(const glm::vec3& position) {
    if (backend_) {
        backend_->setPosition(position);
    }
}

void PhysicsRigidBody::setRotation(const glm::quat& rotation) {
    if (backend_) {
        backend_->setRotation(rotation);
    }
}

void PhysicsRigidBody::setVelocity(const glm::vec3& velocity) {
    if (backend_) {
        backend_->setVelocity(velocity);
    }
}

void PhysicsRigidBody::setAngularVelocity(const glm::vec3& angularVelocity) {
    if (backend_) {
        backend_->setAngularVelocity(angularVelocity);
    }
}

bool PhysicsRigidBody::isGrounded(const glm::vec3& dimensions) const {
    return backend_ ? backend_->isGrounded(dimensions) : false;
}

void PhysicsRigidBody::destroy() {
    if (!backend_) {
        return;
    }
    backend_->destroy();
    backend_.reset();
}

std::uintptr_t PhysicsRigidBody::nativeHandle() const {
    return backend_ ? backend_->nativeHandle() : 0;
}
