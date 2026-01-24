#include "physics/player_controller.hpp"
#include "physics/backend.hpp"

PhysicsPlayerController::PhysicsPlayerController(std::unique_ptr<physics_backend::PhysicsPlayerControllerBackend> backend)
    : backend_(std::move(backend)) {}

PhysicsPlayerController::~PhysicsPlayerController() {
    destroy();
}

glm::vec3 PhysicsPlayerController::getPosition() const {
    return backend_ ? backend_->getPosition() : glm::vec3(0.0f);
}

glm::quat PhysicsPlayerController::getRotation() const {
    return backend_ ? backend_->getRotation() : glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
}

glm::vec3 PhysicsPlayerController::getVelocity() const {
    return backend_ ? backend_->getVelocity() : glm::vec3(0.0f);
}

glm::vec3 PhysicsPlayerController::getAngularVelocity() const {
    return backend_ ? backend_->getAngularVelocity() : glm::vec3(0.0f);
}

glm::vec3 PhysicsPlayerController::getForwardVector() const {
    return backend_ ? backend_->getForwardVector() : glm::vec3(0.0f, 0.0f, -1.0f);
}

void PhysicsPlayerController::setHalfExtents(const glm::vec3& extents) {
    if (backend_) {
        backend_->setHalfExtents(extents);
    }
}

void PhysicsPlayerController::update(float dt) {
    if (backend_) {
        backend_->update(dt);
    }
}

void PhysicsPlayerController::setPosition(const glm::vec3& position) {
    if (backend_) {
        backend_->setPosition(position);
    }
}

void PhysicsPlayerController::setRotation(const glm::quat& rotation) {
    if (backend_) {
        backend_->setRotation(rotation);
    }
}

void PhysicsPlayerController::setVelocity(const glm::vec3& velocity) {
    if (backend_) {
        backend_->setVelocity(velocity);
    }
}

void PhysicsPlayerController::setAngularVelocity(const glm::vec3& angularVelocity) {
    if (backend_) {
        backend_->setAngularVelocity(angularVelocity);
    }
}

bool PhysicsPlayerController::isGrounded() const {
    return backend_ ? backend_->isGrounded() : false;
}

void PhysicsPlayerController::destroy() {
    if (!backend_) {
        return;
    }
    backend_->destroy();
    backend_.reset();
}
