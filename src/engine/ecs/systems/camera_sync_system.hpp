#pragma once

#include "karma/ecs/components.hpp"
#include "karma/ecs/world.hpp"
#include "karma/renderer/renderer_context.hpp"

namespace ecs {

class CameraSyncSystem {
public:
    void update(World &world, engine::renderer::RendererContext &context) {
        const auto &cameras = world.all<CameraComponent>();
        if (cameras.empty()) {
            return;
        }
        const auto &transforms = world.all<Transform>();

        const CameraComponent *selected = nullptr;
        EntityId selectedEntity = kInvalidEntity;
        for (const auto &pair : cameras) {
            if (pair.second.is_primary) {
                selected = &pair.second;
                selectedEntity = pair.first;
                break;
            }
        }
        if (!selected) {
            selected = &cameras.begin()->second;
            selectedEntity = cameras.begin()->first;
        }
        if (!selected) {
            return;
        }
        const auto transformIt = transforms.find(selectedEntity);
        if (transformIt == transforms.end()) {
            return;
        }
        const Transform &transform = transformIt->second;
        context.cameraPosition = transform.position;
        context.cameraRotation = transform.rotation;
        context.fov = selected->fov_degrees;
        context.nearPlane = selected->near_plane;
        context.farPlane = selected->far_plane;
    }
};

} // namespace ecs
