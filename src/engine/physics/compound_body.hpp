#pragma once

#include <vector>
#include <memory>

class btRigidBody;

class PhysicsWorld;

class PhysicsCompoundBody {
public:
    PhysicsCompoundBody() = default;
    PhysicsCompoundBody(PhysicsWorld* world, std::vector<btRigidBody*> bodies);
    PhysicsCompoundBody(const PhysicsCompoundBody&) = delete;
    PhysicsCompoundBody& operator=(const PhysicsCompoundBody&) = delete;
    PhysicsCompoundBody(PhysicsCompoundBody&& other) noexcept;
    PhysicsCompoundBody& operator=(PhysicsCompoundBody&& other) noexcept;
    ~PhysicsCompoundBody();

    bool isValid() const { return world_ != nullptr; }

    void destroy();

    const std::vector<btRigidBody*>& nativeHandles() const { return bodies_; }

private:
    PhysicsWorld* world_ = nullptr;
    std::vector<btRigidBody*> bodies_;
};
