#pragma once

#include "karma/ecs/components.hpp"
#include "karma/ecs/world.hpp"
#include "karma/graphics/device.hpp"

namespace ecs {

class ProceduralMeshSyncSystem {
public:
    void update(World &world, graphics::GraphicsDevice *graphics) {
        if (!graphics) {
            return;
        }
        const auto &procedurals = world.all<ProceduralMesh>();
        const auto &renderMeshes = world.all<RenderMesh>();
        for (const auto &pair : procedurals) {
            const EntityId entity = pair.first;
            const ProceduralMesh &proc = pair.second;
            if (!proc.dirty && renderMeshes.find(entity) != renderMeshes.end()) {
                continue;
            }
            const graphics::MeshId meshId = graphics->createMesh(proc.mesh);
            if (meshId != graphics::kInvalidMesh) {
                world.set(entity, RenderMesh{meshId});
                auto *mutableProc = world.get<ProceduralMesh>(entity);
                if (mutableProc) {
                    mutableProc->dirty = false;
                }
            }
        }
    }
};

} // namespace ecs
