#pragma once

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyID.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <optional>
#include <string>

class PhysicsWorld;

// Lightweight wrapper for immovable physics geometry (e.g., level meshes)
class PhysicsStaticBody {
public:
    PhysicsStaticBody() = default;
    PhysicsStaticBody(PhysicsWorld* world, const JPH::BodyID& bodyId);
    static PhysicsStaticBody fromMesh(PhysicsWorld* world, const std::string& meshPath);
    PhysicsStaticBody(const PhysicsStaticBody&) = delete;
    PhysicsStaticBody& operator=(const PhysicsStaticBody&) = delete;
    PhysicsStaticBody(PhysicsStaticBody&& other) noexcept;
    PhysicsStaticBody& operator=(PhysicsStaticBody&& other) noexcept;
    ~PhysicsStaticBody();

    bool isValid() const { return world_ != nullptr && body_.has_value(); }

    glm::vec3 getPosition() const;
    glm::quat getRotation() const;

    void destroy();

    JPH::BodyID nativeHandle() const;

private:
    PhysicsWorld* world_ = nullptr;
    std::optional<JPH::BodyID> body_;
};
