#pragma once

#include "engine/graphics/types.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace engine::renderer {

struct RenderContext {
    graphics::LayerId mainLayer = 0;
    graphics::RenderTargetId mainTarget = graphics::kDefaultRenderTarget;
    float fov = 60.0f;
    float nearPlane = 0.1f;
    float farPlane = 1000.0f;
    float aspect = 1.0f;
    glm::vec3 cameraPosition{0.0f};
    glm::quat cameraRotation{1.0f, 0.0f, 0.0f, 0.0f};
};

} // namespace engine::renderer
