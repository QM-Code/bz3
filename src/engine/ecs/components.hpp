#pragma once

#include "karma/ecs/types.hpp"
#include "karma/graphics/types.hpp"
#include "karma/physics/types.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>

namespace ecs {

struct Transform {
    glm::vec3 position{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 scale{1.0f};
};

struct RenderMesh {
    uint32_t meshId = 0;
};

struct Material {
    uint32_t materialId = 0;
};

struct RenderEntity {
    graphics::EntityId entityId = graphics::kInvalidEntity;
};

struct RenderLayer {
    graphics::LayerId layer = 0;
};

struct Transparency {
    bool enabled = false;
};

struct ProceduralMesh {
    graphics::MeshData mesh{};
    bool dirty = true;
};

// Karma-style ECS components (data-only, synced later).
struct MeshComponent {
    std::string mesh_key;
};

struct MaterialComponent {
    graphics::MaterialId materialId = graphics::kInvalidMaterial;
};

struct CameraComponent {
    bool is_primary = false;
    float fov_degrees = 60.0f;
    float near_plane = 0.1f;
    float far_plane = 500.0f;
};

struct LightComponent {
    enum class Type { Directional, Point, Spot };
    Type type = Type::Directional;
    glm::vec3 color{1.0f};
    float intensity = 1.0f;
    float range = 10.0f;
    float inner_cone = 0.5f;
    float outer_cone = 0.8f;
    float shadow_extent = 50.0f;
};

struct ColliderComponent {
    enum class Shape { Box, Sphere, Capsule, Mesh };
    Shape shape = Shape::Box;
    glm::vec3 half_extents{0.5f};
    float radius = 0.5f;
    float height = 1.0f;
    std::string mesh_key;
    PhysicsMaterial material{};
};

struct RigidbodyComponent {
    float mass = 1.0f;
    bool kinematic = false;
    glm::vec3 linear_velocity{0.0f};
    glm::vec3 angular_velocity{0.0f};
};

struct AudioSourceComponent {
    std::string clip_key;
    float gain = 1.0f;
    bool spatialized = true;
    bool loop = false;
    bool play_on_start = false;
};

struct AudioListenerComponent {
    bool active = true;
};

} // namespace ecs
