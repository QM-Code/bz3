#include "physics/backends/physx/static_body_physx.hpp"
#include "physics/backends/physx/physics_world_physx.hpp"
#include "engine/geometry/mesh_loader.hpp"
#include <PxPhysicsAPI.h>
#include <spdlog/spdlog.h>
#include <vector>

namespace {
inline glm::vec3 toGlm(const physx::PxVec3& v) { return glm::vec3(v.x, v.y, v.z); }
}

namespace physics_backend {

PhysicsStaticBodyPhysX::PhysicsStaticBodyPhysX(PhysicsWorldPhysX* world, physx::PxRigidActor* actor, physx::PxTriangleMesh* mesh)
    : world_(world), actor_(actor), mesh_(mesh) {}

PhysicsStaticBodyPhysX::~PhysicsStaticBodyPhysX() {
    destroy();
}

bool PhysicsStaticBodyPhysX::isValid() const {
    return world_ != nullptr && actor_ != nullptr;
}

glm::vec3 PhysicsStaticBodyPhysX::getPosition() const {
    if (!actor_) return glm::vec3(0.0f);
    return toGlm(actor_->getGlobalPose().p);
}

glm::quat PhysicsStaticBodyPhysX::getRotation() const {
    if (!actor_) return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    const physx::PxQuat q = actor_->getGlobalPose().q;
    return glm::quat(q.w, q.x, q.y, q.z);
}

void PhysicsStaticBodyPhysX::destroy() {
    if (!actor_ || !world_ || !world_->scene()) {
        actor_ = nullptr;
        world_ = nullptr;
        return;
    }
    world_->scene()->removeActor(*actor_);
    actor_->release();
    actor_ = nullptr;
    if (mesh_) {
        mesh_->release();
        mesh_ = nullptr;
    }
    world_ = nullptr;
}

std::uintptr_t PhysicsStaticBodyPhysX::nativeHandle() const {
    return reinterpret_cast<std::uintptr_t>(actor_);
}

std::unique_ptr<PhysicsStaticBodyBackend> PhysicsStaticBodyPhysX::fromMesh(PhysicsWorldPhysX* world, const std::string& meshPath) {
    if (!world || !world->physics() || !world->scene()) {
        return std::make_unique<PhysicsStaticBodyPhysX>();
    }

    std::vector<MeshLoader::MeshData> meshes = MeshLoader::loadGLB(meshPath);
    if (meshes.empty()) {
        spdlog::warn("PhysX static mesh: no meshes found at {}", meshPath);
        return std::make_unique<PhysicsStaticBodyPhysX>();
    }

    std::vector<physx::PxVec3> vertices;
    std::vector<physx::PxU32> indices;
    vertices.reserve(1024);
    indices.reserve(2048);

    physx::PxU32 base = 0;
    for (const auto& mesh : meshes) {
        for (const auto& v : mesh.vertices) {
            vertices.emplace_back(v.x, v.y, v.z);
        }
        const auto& idx = mesh.indices;
        if (idx.size() % 3 != 0) {
            spdlog::warn("PhysX static mesh: Mesh {} has non-multiple-of-3 indices; skipping remainder", meshPath);
        }
        for (size_t i = 0; i + 2 < idx.size(); i += 3) {
            indices.push_back(base + static_cast<physx::PxU32>(idx[i]));
            indices.push_back(base + static_cast<physx::PxU32>(idx[i + 1]));
            indices.push_back(base + static_cast<physx::PxU32>(idx[i + 2]));
        }
        base = static_cast<physx::PxU32>(vertices.size());
    }

    if (indices.empty() || vertices.empty()) {
        spdlog::warn("PhysX static mesh: no triangles generated for {}", meshPath);
        return std::make_unique<PhysicsStaticBodyPhysX>();
    }

    physx::PxTriangleMeshDesc meshDesc;
    meshDesc.points.count = static_cast<physx::PxU32>(vertices.size());
    meshDesc.points.stride = sizeof(physx::PxVec3);
    meshDesc.points.data = vertices.data();
    meshDesc.triangles.count = static_cast<physx::PxU32>(indices.size() / 3);
    meshDesc.triangles.stride = 3 * sizeof(physx::PxU32);
    meshDesc.triangles.data = indices.data();

    physx::PxCookingParams cookingParams(world->physics()->getTolerancesScale());
    physx::PxTriangleMesh* triangleMesh = PxCreateTriangleMesh(
        cookingParams,
        meshDesc,
        world->physics()->getPhysicsInsertionCallback());
    if (!triangleMesh) {
        spdlog::error("PhysX static mesh: failed to create triangle mesh for {}", meshPath);
        return std::make_unique<PhysicsStaticBodyPhysX>();
    }

    physx::PxMaterial* material = world->defaultMaterial();
    if (!material) {
        triangleMesh->release();
        return std::make_unique<PhysicsStaticBodyPhysX>();
    }

    physx::PxRigidStatic* actor = world->physics()->createRigidStatic(physx::PxTransform(physx::PxIdentity));
    if (!actor) {
        triangleMesh->release();
        return std::make_unique<PhysicsStaticBodyPhysX>();
    }
    physx::PxShape* shape = world->physics()->createShape(physx::PxTriangleMeshGeometry(triangleMesh), *material);
    if (!shape) {
        actor->release();
        triangleMesh->release();
        return std::make_unique<PhysicsStaticBodyPhysX>();
    }
    actor->attachShape(*shape);
    shape->release();

    world->scene()->addActor(*actor);
    return std::make_unique<PhysicsStaticBodyPhysX>(world, actor, triangleMesh);
}

} // namespace physics_backend
