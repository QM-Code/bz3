#include "physics/static_body.hpp"
#include "physics/backend.hpp"

PhysicsStaticBody::PhysicsStaticBody(std::unique_ptr<physics_backend::PhysicsStaticBodyBackend> backend)
    : backend_(std::move(backend)) {}

PhysicsStaticBody::~PhysicsStaticBody() {
    destroy();
}

bool PhysicsStaticBody::isValid() const {
    return backend_ && backend_->isValid();
}

glm::vec3 PhysicsStaticBody::getPosition() const {
    return backend_ ? backend_->getPosition() : glm::vec3(0.0f);
}

glm::quat PhysicsStaticBody::getRotation() const {
    return backend_ ? backend_->getRotation() : glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
}

void PhysicsStaticBody::destroy() {
    if (!backend_) {
        return;
    }
    backend_->destroy();
    backend_.reset();
}

std::uintptr_t PhysicsStaticBody::nativeHandle() const {
    return backend_ ? backend_->nativeHandle() : 0;
}
