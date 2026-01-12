#include "engine/physics/compound_body.hpp"
#include "engine/physics/physics_world.hpp"
#include <bullet/btBulletDynamicsCommon.h>

PhysicsCompoundBody::PhysicsCompoundBody(PhysicsWorld* world, std::vector<btRigidBody*> bodies)
    : world_(world), bodies_(std::move(bodies)) {}

PhysicsCompoundBody::PhysicsCompoundBody(PhysicsCompoundBody&& other) noexcept {
    world_ = other.world_;
    bodies_ = std::move(other.bodies_);
    other.world_ = nullptr;
    other.bodies_.clear();
}

PhysicsCompoundBody& PhysicsCompoundBody::operator=(PhysicsCompoundBody&& other) noexcept {
    if (this == &other) {
        return *this;
    }

    destroy();

    world_ = other.world_;
    bodies_ = std::move(other.bodies_);
    other.world_ = nullptr;
    other.bodies_.clear();
    return *this;
}

PhysicsCompoundBody::~PhysicsCompoundBody() {
    destroy();
}

void PhysicsCompoundBody::destroy() {
    if (!isValid()) {
        return;
    }

    for (btRigidBody* body : bodies_) {
        world_->removeBody(body);
    }

    bodies_.clear();
    world_ = nullptr;
}
