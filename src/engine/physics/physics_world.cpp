#include "engine/physics/physics_world.hpp"
#include "engine/mesh_loader.hpp"
#include <bullet/btBulletDynamicsCommon.h>
#include <spdlog/spdlog.h>

namespace {
constexpr int NUM_SUBSTEPS = 10;
constexpr float FIXED_TIMESTEP = 1.0f / 60.0f;
}

PhysicsWorld::PhysicsWorld() {
    broadphase_ = new btDbvtBroadphase();
    collisionConfig_ = new btDefaultCollisionConfiguration();
    dispatcher_ = new btCollisionDispatcher(collisionConfig_);
    solver_ = new btSequentialImpulseConstraintSolver();
    world_ = new btDiscreteDynamicsWorld(dispatcher_, broadphase_, solver_, collisionConfig_);
    world_->setGravity(btVector3(0, -9.8f, 0));
}

PhysicsWorld::~PhysicsWorld() {
    delete world_;
    delete solver_;
    delete dispatcher_;
    delete collisionConfig_;
    delete broadphase_;
}

void PhysicsWorld::update(float deltaTime) {
    world_->stepSimulation(deltaTime, NUM_SUBSTEPS, FIXED_TIMESTEP);
}

void PhysicsWorld::setGravity(float gravity) {
    world_->setGravity(btVector3(0, gravity, 0));
}

PhysicsRigidBody PhysicsWorld::createBoxBody(const glm::vec3& halfExtents,
                                             float mass,
                                             const glm::vec3& position,
                                             const PhysicsMaterial& material) {
    btBoxShape* shape = new btBoxShape(btVector3(halfExtents.x, halfExtents.y, halfExtents.z));

    btTransform transform;
    transform.setIdentity();
    transform.setOrigin(btVector3(position.x, position.y, position.z));

    btVector3 inertia(0, 0, 0);
    if (mass > 0.0f) {
        shape->calculateLocalInertia(mass, inertia);
    }

    btDefaultMotionState* motionState = new btDefaultMotionState(transform);
    btRigidBody::btRigidBodyConstructionInfo info(mass, motionState, shape, inertia);
    btRigidBody* body = new btRigidBody(info);

    body->setFriction(material.friction);
    body->setRestitution(material.restitution);
    body->setRollingFriction(material.rollingFriction);
    body->setSpinningFriction(material.spinningFriction);

    world_->addRigidBody(body);

    return PhysicsRigidBody(this, body);
}

PhysicsRigidBody PhysicsWorld::createPlayer(const glm::vec3& size) {
    glm::vec3 halfExtents = size * 0.5f;

    PhysicsMaterial material;
    material.friction = 0.0f;
    material.restitution = 0.0f;
    material.rollingFriction = 0.0f;
    material.spinningFriction = 0.0f;

    PhysicsRigidBody body = createBoxBody(halfExtents, 1.0f, glm::vec3(0.0f, 2.0f, 0.0f), material);
    if (btRigidBody* btBody = body.nativeHandle()) {
        btBody->setDamping(0, 0);
        btBody->setAngularFactor(btVector3(0, 0.1f, 0));
        btBody->setLinearFactor(btVector3(1, 1, 1));
    }

    spdlog::info("Created player physics rigid body");
    return body;
}

PhysicsCompoundBody PhysicsWorld::createStaticMesh(const std::string& meshPath, float mass) {
    std::vector<MeshLoader::MeshData> meshes = MeshLoader::loadGLB(meshPath);
    if (meshes.empty()) {
        spdlog::warn("PhysicsWorld::createStaticMesh: No meshes found at {}", meshPath);
        return PhysicsCompoundBody();
    }

    std::vector<btRigidBody*> bodies;
    bodies.reserve(meshes.size());

    for (auto& mesh : meshes) {
        btConvexHullShape* shape = new btConvexHullShape();
        for (const auto& v : mesh.vertices) {
            shape->addPoint(btVector3(v.x, v.y, v.z));
        }

        btVector3 inertia(0, 0, 0);
        if (mass > 0.0f) {
            shape->calculateLocalInertia(mass, inertia);
        }

        btDefaultMotionState* motionState = new btDefaultMotionState(
            btTransform(btQuaternion(0, 0, 0, 1), btVector3(0, 0, 0))
        );

        btRigidBody::btRigidBodyConstructionInfo info(mass, motionState, shape, inertia);
        btRigidBody* body = new btRigidBody(info);

        body->setFriction(0.0f);
        body->setRestitution(0.0f);
        body->setRollingFriction(0.0f);
        body->setSpinningFriction(0.0f);

        world_->addRigidBody(body);
        bodies.push_back(body);
    }

    return PhysicsCompoundBody(this, std::move(bodies));
}

bool PhysicsWorld::raycast(const glm::vec3& from, const glm::vec3& to, glm::vec3& hitPoint, glm::vec3& hitNormal) const {
    btVector3 btFrom(from.x, from.y, from.z);
    btVector3 btTo(to.x, to.y, to.z);

    btCollisionWorld::ClosestRayResultCallback rayCallback(btFrom, btTo);
    world_->rayTest(btFrom, btTo, rayCallback);

    if (rayCallback.hasHit()) {
        btVector3 hitPos = rayCallback.m_hitPointWorld;
        btVector3 hitNorm = rayCallback.m_hitNormalWorld;

        hitPoint = glm::vec3(hitPos.x(), hitPos.y(), hitPos.z());
        hitNormal = glm::vec3(hitNorm.x(), hitNorm.y(), hitNorm.z());
        return true;
    }
    return false;
}

void PhysicsWorld::removeBody(btRigidBody* body) const {
    if (!body) {
        return;
    }

    world_->removeRigidBody(body);
    delete body->getMotionState();
    delete body->getCollisionShape();
    delete body;
}
