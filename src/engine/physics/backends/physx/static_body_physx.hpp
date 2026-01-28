#pragma once

#include "physics/backend.hpp"
#include <PxPhysicsAPI.h>

namespace physics_backend {

class PhysicsWorldPhysX;

class PhysicsStaticBodyPhysX final : public PhysicsStaticBodyBackend {
public:
    PhysicsStaticBodyPhysX() = default;
    PhysicsStaticBodyPhysX(PhysicsWorldPhysX* world, physx::PxRigidActor* actor, physx::PxTriangleMesh* mesh);
    ~PhysicsStaticBodyPhysX() override;

    bool isValid() const override;
    glm::vec3 getPosition() const override;
    glm::quat getRotation() const override;
    void destroy() override;
    std::uintptr_t nativeHandle() const override;

    static std::unique_ptr<PhysicsStaticBodyBackend> fromMesh(PhysicsWorldPhysX* world, const std::string& meshPath);

private:
    PhysicsWorldPhysX* world_ = nullptr;
    physx::PxRigidActor* actor_ = nullptr;
    physx::PxTriangleMesh* mesh_ = nullptr;
};

} // namespace physics_backend
