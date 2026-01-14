#include "engine/physics/physics_world.hpp"
#include "engine/mesh_loader.hpp"
#include <btBulletDynamicsCommon.h>
#include <BulletCollision/CollisionShapes/btTriangleInfoMap.h>
#include <BulletCollision/CollisionShapes/btBvhTriangleMeshShape.h>
#include <BulletCollision/CollisionShapes/btTriangleMesh.h>
#include <BulletCollision/CollisionDispatch/btInternalEdgeUtility.h>
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
    if (playerController_) {
        playerController_->update(deltaTime);
    }
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

PhysicsPlayerController& PhysicsWorld::createPlayer(const glm::vec3& size) {
    const glm::vec3 halfExtents = size * 0.5f;
    PhysicsPlayerController controller(this, halfExtents, glm::vec3(0.0f, 2.0f, 0.0f));
    controller.setRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
    controller.setVelocity(glm::vec3(0.0f));
    controller.setAngularVelocity(glm::vec3(0.0f));
    playerController_ = std::make_unique<PhysicsPlayerController>(std::move(controller));
    spdlog::info("Created kinematic player controller");
    return *playerController_;
}

PhysicsPlayerController& PhysicsWorld::createPlayer() {
	return createPlayer(glm::vec3(1.0f, 2.0f, 1.0f));
}

PhysicsCompoundBody PhysicsWorld::createStaticMesh(const std::string& meshPath, float mass) {
    std::vector<MeshLoader::MeshData> meshes = MeshLoader::loadGLB(meshPath);
    if (meshes.empty()) {
        spdlog::warn("PhysicsWorld::createStaticMesh: No meshes found at {}", meshPath);
        return PhysicsCompoundBody();
    }

    // Static geometry should be immovable
    if (mass > 0.0f) {
        spdlog::warn("PhysicsWorld::createStaticMesh: mass {} ignored; static meshes must be immovable (mass=0)", mass);
    }

    std::vector<btRigidBody*> bodies;
    bodies.reserve(meshes.size());

    for (auto& mesh : meshes) {
        // Build triangle mesh from indexed geometry
        auto triMesh = new btTriangleMesh();
        const auto& verts = mesh.vertices;
        const auto& idx   = mesh.indices;

        if (idx.size() % 3 != 0) {
            spdlog::warn("Mesh {} has non-multiple-of-3 indices; skipping", meshPath);
            continue;
        }

        for (size_t i = 0; i + 2 < idx.size(); i += 3) {
            const auto& a = verts[idx[i]];
            const auto& b = verts[idx[i + 1]];
            const auto& c = verts[idx[i + 2]];
            triMesh->addTriangle(
                btVector3(a.x, a.y, a.z),
                btVector3(b.x, b.y, b.z),
                btVector3(c.x, c.y, c.z),
                /* removeDuplicateVertices */ true);
        }

        // Static optimized triangle mesh shape (BVH)
        btBvhTriangleMeshShape* shape = new btBvhTriangleMeshShape(triMesh, /* useQuantizedAabbCompression */ true);
        shape->setMargin(0.01f);  // small margin to smooth contacts

        // Generate internal edge info to avoid catching on triangle edges
        auto* triInfoMap = new btTriangleInfoMap();
        btGenerateInternalEdgeInfo(shape, triInfoMap);

        btDefaultMotionState* motionState =
            new btDefaultMotionState(btTransform(btQuaternion(0, 0, 0, 1), btVector3(0, 0, 0)));

        btRigidBody::btRigidBodyConstructionInfo info(0.0f, motionState, shape, btVector3(0, 0, 0));
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

        // Normalize for stable reflections
        const btScalar len = hitNorm.length();
        if (len > SIMD_EPSILON) hitNorm /= len;

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
