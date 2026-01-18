#include "engine/physics/static_body.hpp"
#include "engine/physics/physics_world.hpp"
#include "engine/mesh_loader.hpp"
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Math/Float3.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <spdlog/spdlog.h>
#include <vector>

using namespace JPH;

namespace {
template <class TVec>
inline glm::vec3 toGlm(const TVec& v) { return glm::vec3(static_cast<float>(v.GetX()), static_cast<float>(v.GetY()), static_cast<float>(v.GetZ())); }
}

PhysicsStaticBody PhysicsStaticBody::fromMesh(PhysicsWorld* world, const std::string& meshPath) {
    if (!world || !world->physicsSystem()) return PhysicsStaticBody();

    std::vector<MeshLoader::MeshData> meshes = MeshLoader::loadGLB(meshPath);
    if (meshes.empty()) {
        spdlog::warn("PhysicsStaticBody::fromMesh: No meshes found at {}", meshPath);
        return PhysicsStaticBody();
    }

    using JPH::VertexList;
    using JPH::IndexedTriangleList;

    VertexList vertices;
    IndexedTriangleList triangles;
    vertices.reserve(1024);
    triangles.reserve(2048);

    uint32_t base = 0;
    for (const auto& mesh : meshes) {
        for (const auto& v : mesh.vertices) {
            vertices.push_back(JPH::Float3(v.x, v.y, v.z));
        }

        const auto& idx = mesh.indices;
        if (idx.size() % 3 != 0) {
            spdlog::warn("PhysicsStaticBody::fromMesh: Mesh {} has non-multiple-of-3 indices; skipping remainder", meshPath);
        }
        for (size_t i = 0; i + 2 < idx.size(); i += 3) {
            triangles.push_back(JPH::IndexedTriangle(base + idx[i], base + idx[i + 1], base + idx[i + 2]));
        }

        base = static_cast<uint32_t>(vertices.size());
    }

    JPH::MeshShapeSettings meshSettings(std::move(vertices), std::move(triangles));
    auto shapeResult = meshSettings.Create();
    if (shapeResult.HasError()) {
        spdlog::error("PhysicsStaticBody::fromMesh: Failed to create mesh shape: {}", shapeResult.GetError().c_str());
        return PhysicsStaticBody();
    }

    JPH::RefConst<JPH::Shape> shape = shapeResult.Get();
    JPH::BodyCreationSettings settings(shape,
                                      JPH::RVec3::sZero(),
                                      JPH::Quat::sIdentity(),
                                      JPH::EMotionType::Static,
                                      PhysicsLayers::NonMoving);

    JPH::BodyInterface& bi = world->physicsSystem()->GetBodyInterface();
    JPH::Body* body = bi.CreateBody(settings);
    if (!body) {
        spdlog::error("PhysicsStaticBody::fromMesh: Failed to create body");
        return PhysicsStaticBody();
    }

    bi.AddBody(body->GetID(), JPH::EActivation::DontActivate);
    return PhysicsStaticBody(world, body->GetID());
}

PhysicsStaticBody::PhysicsStaticBody(PhysicsWorld* world, const BodyID& bodyId)
    : world_(world), body_(bodyId) {}

PhysicsStaticBody::PhysicsStaticBody(PhysicsStaticBody&& other) noexcept {
    world_ = other.world_;
    body_ = other.body_;
    other.world_ = nullptr;
    other.body_.reset();
}

PhysicsStaticBody& PhysicsStaticBody::operator=(PhysicsStaticBody&& other) noexcept {
    if (this == &other) return *this;

    destroy();
    world_ = other.world_;
    body_ = other.body_;
    other.world_ = nullptr;
    other.body_.reset();
    return *this;
}

PhysicsStaticBody::~PhysicsStaticBody() {
    destroy();
}

void PhysicsStaticBody::destroy() {
    if (!world_ || !body_.has_value()) return;
    world_->removeBody(*body_);
    body_.reset();
    world_ = nullptr;
}

glm::vec3 PhysicsStaticBody::getPosition() const {
    const BodyInterface& bi = world_->physicsSystem()->GetBodyInterface();
    RVec3 pos = bi.GetCenterOfMassPosition(*body_);
    return toGlm(pos);
}

glm::quat PhysicsStaticBody::getRotation() const {
    const BodyInterface& bi = world_->physicsSystem()->GetBodyInterface();
    Quat rot = bi.GetRotation(*body_);
    return glm::quat(rot.GetW(), rot.GetX(), rot.GetY(), rot.GetZ());
}

JPH::BodyID PhysicsStaticBody::nativeHandle() const {
    return body_.value_or(JPH::BodyID());
}
