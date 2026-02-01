#pragma once

#include "karma/ecs/components.hpp"
#include "karma/ecs/world.hpp"
#include "karma/graphics/resources.hpp"
#include "karma/graphics/types.hpp"

namespace ecs {

class RenderSyncSystem {
public:
    void update(World &world,
                graphics::ResourceRegistry *resources,
                graphics::MaterialId defaultMaterial) {
        if (!resources) {
            return;
        }
        for (const auto &pair : world.all<MeshComponent>()) {
            const EntityId entity = pair.first;
            const MeshComponent &mesh = pair.second;
            if (mesh.mesh_key.empty()) {
                continue;
            }
            const auto &renderMeshes = world.all<RenderMesh>();
            const auto &renderEntities = world.all<RenderEntity>();
            if (renderEntities.find(entity) == renderEntities.end() &&
                renderMeshes.find(entity) == renderMeshes.end()) {
                const graphics::MeshId meshId = resources->loadMesh(mesh.mesh_key);
                if (meshId != graphics::kInvalidMesh) {
                    world.set(entity, RenderMesh{meshId});
                }
            }
        }
        for (const auto &pair : world.all<MaterialComponent>()) {
            const EntityId entity = pair.first;
            const MaterialComponent &material = pair.second;
            if (material.materialId == graphics::kInvalidMaterial) {
                continue;
            }
            world.set(entity, Material{material.materialId});
        }
        if (defaultMaterial != graphics::kInvalidMaterial) {
            for (const auto &pair : world.all<RenderMesh>()) {
                const EntityId entity = pair.first;
                const auto &materials = world.all<Material>();
                if (materials.find(entity) == materials.end()) {
                    world.set(entity, Material{defaultMaterial});
                }
            }
        }
    }
};

} // namespace ecs
