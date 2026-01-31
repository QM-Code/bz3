#pragma once

#include "karma/ecs/types.hpp"
#include "karma/graphics/types.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

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

} // namespace ecs
