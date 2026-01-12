#pragma once

#include "engine/physics/rigid_body.hpp"
#include "engine/physics/compound_body.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>

class btBroadphaseInterface;
class btDefaultCollisionConfiguration;
class btCollisionDispatcher;
class btSequentialImpulseConstraintSolver;
class btDiscreteDynamicsWorld;
class btCollisionShape;
class btRigidBody;

struct PhysicsMaterial {
    float friction = 0.0f;
    float restitution = 0.0f;
    float rollingFriction = 0.0f;
    float spinningFriction = 0.0f;
};

class PhysicsWorld {
public:
    PhysicsWorld();
    ~PhysicsWorld();

    PhysicsWorld(const PhysicsWorld&) = delete;
    PhysicsWorld& operator=(const PhysicsWorld&) = delete;

    void update(float deltaTime);

    void setGravity(float gravity);

    PhysicsRigidBody createBoxBody(const glm::vec3& halfExtents,
                                   float mass,
                                   const glm::vec3& position,
                                   const PhysicsMaterial& material);

    PhysicsRigidBody createPlayer(const glm::vec3& size);

    PhysicsCompoundBody createStaticMesh(const std::string& meshPath, float mass);

    bool raycast(const glm::vec3& from, const glm::vec3& to, glm::vec3& hitPoint, glm::vec3& hitNormal) const;

private:
    friend class PhysicsRigidBody;
    friend class PhysicsCompoundBody;

    void removeBody(btRigidBody* body) const;

    btBroadphaseInterface* broadphase_ = nullptr;
    btDefaultCollisionConfiguration* collisionConfig_ = nullptr;
    btCollisionDispatcher* dispatcher_ = nullptr;
    btSequentialImpulseConstraintSolver* solver_ = nullptr;
    btDiscreteDynamicsWorld* world_ = nullptr;
};
