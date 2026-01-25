#pragma once

#include "physics/backend.hpp"
#include <cstdint>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <memory>

// Lightweight wrapper for immovable physics geometry (e.g., level meshes)
class PhysicsStaticBody {
public:
    PhysicsStaticBody() = default;
    explicit PhysicsStaticBody(std::unique_ptr<physics_backend::PhysicsStaticBodyBackend> backend);
    PhysicsStaticBody(const PhysicsStaticBody&) = delete;
    PhysicsStaticBody& operator=(const PhysicsStaticBody&) = delete;
    PhysicsStaticBody(PhysicsStaticBody&& other) noexcept = default;
    PhysicsStaticBody& operator=(PhysicsStaticBody&& other) noexcept = default;
    ~PhysicsStaticBody();

    bool isValid() const;

    glm::vec3 getPosition() const;
    glm::quat getRotation() const;

    void destroy();

    std::uintptr_t nativeHandle() const;

private:
    std::unique_ptr<physics_backend::PhysicsStaticBodyBackend> backend_;
};
