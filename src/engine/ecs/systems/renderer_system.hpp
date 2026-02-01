#pragma once

#include "karma/ecs/components.hpp"
#include "karma/ecs/world.hpp"
#include "karma/graphics/device.hpp"
#include "karma/graphics/types.hpp"
#include "spdlog/spdlog.h"
#include <cstdlib>
#include <unordered_map>

namespace graphics {
class GraphicsDevice;
}

namespace ecs {

class RendererSystem {
public:
    void setDefaultMaterial(graphics::MaterialId material) { defaultMaterial_ = material; }

    void update(World &world, graphics::GraphicsDevice *graphics, float dt) {
        if (!graphics) {
            return;
        }
        if (debugEnabled()) {
            debugAccum_ += dt;
            if (debugAccum_ >= 1.0f) {
                debugAccum_ = 0.0f;
                spdlog::info("RendererSystem: entities={} meshes={} renderEntities={}",
                             world.all<Transform>().size(),
                             world.all<RenderMesh>().size(),
                             world.all<RenderEntity>().size());
            }
        }
        for (const EntityId entity : world.consumeDestroyed()) {
            graphics::EntityId gfxEntity = graphics::kInvalidEntity;
            if (auto it = entities_.find(entity); it != entities_.end()) {
                gfxEntity = it->second.entity;
                entities_.erase(it);
            } else if (const auto &renderEntities = world.all<RenderEntity>();
                       renderEntities.find(entity) != renderEntities.end()) {
                gfxEntity = renderEntities.at(entity).entityId;
            }
            if (gfxEntity != graphics::kInvalidEntity) {
                graphics->destroyEntity(gfxEntity);
            }
            world.remove<RenderEntity>(entity);
            world.remove<RenderMesh>(entity);
            world.remove<Material>(entity);
            world.remove<Transform>(entity);
            world.remove<RenderLayer>(entity);
        }
        const auto &meshes = world.all<RenderMesh>();
        const auto &materials = world.all<Material>();
        const auto &renderEntities = world.all<RenderEntity>();
        const auto &transparency = world.all<Transparency>();
        const auto &layers = world.all<RenderLayer>();
        const auto &meshComponents = world.all<MeshComponent>();
        for (const auto &pair : world.all<Transform>()) {
            const EntityId entity = pair.first;
            const Transform &transform = pair.second;
            graphics::EntityId gfxEntity = graphics::kInvalidEntity;
            if (auto reIt = renderEntities.find(entity); reIt != renderEntities.end()) {
                gfxEntity = reIt->second.entityId;
            }
            graphics::LayerId desiredLayer = 0;
            if (auto layerIt = layers.find(entity); layerIt != layers.end()) {
                desiredLayer = layerIt->second.layer;
            }
            if (auto handleIt = entities_.find(entity); handleIt != entities_.end()) {
                if (handleIt->second.layer != desiredLayer) {
                    if (handleIt->second.entity != graphics::kInvalidEntity) {
                        graphics->destroyEntity(handleIt->second.entity);
                    }
                    handleIt->second.entity = graphics::kInvalidEntity;
                    handleIt->second.layer = desiredLayer;
                    gfxEntity = graphics::kInvalidEntity;
                }
            }

            if (gfxEntity == graphics::kInvalidEntity) {
                const auto meshIt = meshes.find(entity);
                const auto meshKeyIt = meshComponents.find(entity);
                if (meshIt == meshes.end() && meshKeyIt == meshComponents.end()) {
                    continue;
                }
                gfxEntity = graphics->createEntity(desiredLayer);
                entities_.insert({entity, RenderHandle{gfxEntity, desiredLayer}});
                if (meshKeyIt != meshComponents.end() && !meshKeyIt->second.mesh_key.empty()) {
                    graphics->setEntityModel(gfxEntity, meshKeyIt->second.mesh_key, graphics::kInvalidMaterial);
                } else if (meshIt != meshes.end()) {
                    graphics->setEntityMesh(gfxEntity, meshIt->second.meshId, graphics::kInvalidMaterial);
                }
                world.set(entity, RenderEntity{gfxEntity});
            }

            if (gfxEntity != graphics::kInvalidEntity) {
                if (auto meshKeyIt = meshComponents.find(entity);
                    meshKeyIt != meshComponents.end() && !meshKeyIt->second.mesh_key.empty()) {
                    graphics::MaterialId material = graphics::kInvalidMaterial;
                    if (auto materialIt = materials.find(entity); materialIt != materials.end()) {
                        material = materialIt->second.materialId;
                    }
                    graphics->setEntityModel(gfxEntity, meshKeyIt->second.mesh_key, material);
                } else if (auto meshIt = meshes.find(entity); meshIt != meshes.end()) {
                    graphics::MaterialId material = defaultMaterial_;
                    if (auto materialIt = materials.find(entity); materialIt != materials.end()) {
                        material = materialIt->second.materialId;
                    }
                    graphics->setEntityMesh(gfxEntity, meshIt->second.meshId, material);
                }
                if (auto transIt = transparency.find(entity); transIt != transparency.end()) {
                    graphics->setTransparency(gfxEntity, transIt->second.enabled);
                }
                graphics->setPosition(gfxEntity, transform.position);
                graphics->setRotation(gfxEntity, transform.rotation);
                graphics->setScale(gfxEntity, transform.scale);
            } else if (auto meshIt = meshes.find(entity); meshIt != meshes.end()) {
                spdlog::warn("RendererSystem: failed to create render entity for ECS id {}", entity);
            }
        }
    }

private:
    struct RenderHandle {
        graphics::EntityId entity = graphics::kInvalidEntity;
        graphics::LayerId layer = 0;
    };
    std::unordered_map<EntityId, RenderHandle> entities_;
    graphics::MaterialId defaultMaterial_ = graphics::kInvalidMaterial;
    float debugAccum_ = 0.0f;

    bool debugEnabled() const {
        if (debugCached_ != -1) {
            return debugCached_ == 1;
        }
        debugCached_ = std::getenv("KARMA_ECS_RENDER_DEBUG") ? 1 : 0;
        return debugCached_ == 1;
    }

    mutable int debugCached_ = -1;
};

} // namespace ecs
