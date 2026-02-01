#include "renderer/renderer.hpp"
#include "renderer/radar_components.hpp"
#include "karma/graphics/resources.hpp"

#include "karma/platform/window.hpp"
#include "spdlog/spdlog.h"
#include <unordered_set>

Renderer::Renderer(platform::Window &windowIn)
    : window(&windowIn) {
    core_ = std::make_unique<engine::renderer::RendererCore>(windowIn);
    radarRenderer_ = std::make_unique<game::renderer::RadarRenderer>(core_->device(), core_->scene());

}

Renderer::~Renderer() = default;

void Renderer::renderRadar(const glm::vec3 &cameraPosition, const glm::quat &cameraRotation) {
    if (!core_) {
        return;
    }
    if (ecsRadarSyncEnabled && ecsWorld && radarRenderer_) {
        syncEcsRadar();
    }
    if (radarRenderer_) {
        radarRenderer_->render(cameraPosition, cameraRotation);
    }
}

void Renderer::setEcsWorld(ecs::World *world) {
    ecsWorld = world;
}

void Renderer::syncEcsRadar() {
    const auto &radarTags = ecsWorld->all<game::renderer::RadarRenderable>();
    if (radarTags.empty()) {
        if (!radarEcsEntities_.empty()) {
            for (const auto &entry : radarEcsEntities_) {
                radarRenderer_->destroy(entry.second.id);
            }
            radarEcsEntities_.clear();
        }
        return;
    }

    const auto &meshes = ecsWorld->all<ecs::MeshComponent>();
    const auto &transforms = ecsWorld->all<ecs::Transform>();
    std::unordered_set<ecs::EntityId> seen;
    seen.reserve(radarTags.size());

    for (const auto &pair : radarTags) {
        const ecs::EntityId entity = pair.first;
        if (!pair.second.enabled) {
            continue;
        }
        auto meshIt = meshes.find(entity);
        if (meshIt == meshes.end()) {
            continue;
        }
        const std::string &meshKey = meshIt->second.mesh_key;
        if (meshKey.empty()) {
            continue;
        }

        seen.insert(entity);
        auto it = radarEcsEntities_.find(entity);
        if (it == radarEcsEntities_.end()) {
            const render_id id = nextId++;
            radarRenderer_->setModel(id, meshKey, true);
            radarRenderer_->setPosition(id, glm::vec3(0.0f));
            radarRenderer_->setRotation(id, glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
            radarRenderer_->setScale(id, glm::vec3(1.0f));
            radarEcsEntities_.insert({entity, RadarEcsEntry{id, meshKey}});
            spdlog::info("Renderer: ECS radar sync created (ecs_entity={}, render_id={}, mesh={})",
                         entity, id, meshKey);
        } else if (it->second.mesh_key != meshKey) {
            radarRenderer_->setModel(it->second.id, meshKey, true);
            it->second.mesh_key = meshKey;
            spdlog::info("Renderer: ECS radar sync updated (ecs_entity={}, render_id={}, mesh={})",
                         entity, it->second.id, meshKey);
        }

        if (auto xformIt = transforms.find(entity); xformIt != transforms.end()) {
            const ecs::Transform &xform = xformIt->second;
            const auto &entry = radarEcsEntities_.find(entity)->second;
            radarRenderer_->setPosition(entry.id, xform.position);
            radarRenderer_->setRotation(entry.id, xform.rotation);
            radarRenderer_->setScale(entry.id, xform.scale);
        }
    }

    for (auto it = radarEcsEntities_.begin(); it != radarEcsEntities_.end(); ) {
        if (seen.find(it->first) == seen.end()) {
            radarRenderer_->destroy(it->second.id);
            it = radarEcsEntities_.erase(it);
        } else {
            ++it;
        }
    }
}

render_id Renderer::createRadarId() {
    return nextId++;
}

void Renderer::setRadarCircleGraphic(render_id id, float radius) {
    if (radarRenderer_) {
        radarRenderer_->setRadarCircleGraphic(id, radius);
    }
}

void Renderer::setRadarFOVLinesAngle(float fovDegrees) {
    if (radarRenderer_) {
        radarRenderer_->setFovDegrees(fovDegrees);
    }
}

void Renderer::destroy(render_id id) {
    if (radarRenderer_) {
        radarRenderer_->destroy(id);
    }
}

void Renderer::setPosition(render_id id, const glm::vec3 &position) {
    if (radarRenderer_) {
        radarRenderer_->setPosition(id, position);
    }
}

void Renderer::setRotation(render_id id, const glm::quat &rotation) {
    if (radarRenderer_) {
        radarRenderer_->setRotation(id, rotation);
    }
}

void Renderer::setScale(render_id id, const glm::vec3 &scale) {
    if (radarRenderer_) {
        radarRenderer_->setScale(id, scale);
    }
}

void Renderer::setVisible(render_id id, bool visible) {
    if (radarRenderer_) {
        radarRenderer_->setVisible(id, visible);
    }
}

void Renderer::setTransparency(render_id id, bool transparency) {
    (void)id;
    (void)transparency;
}

graphics::TextureHandle Renderer::getRadarTexture() const {
    return radarRenderer_ ? radarRenderer_->getRadarTexture() : graphics::TextureHandle{};
}

graphics_backend::UiRenderTargetBridge* Renderer::getUiRenderTargetBridge() const {
    return core_ ? core_->device().getUiRenderTargetBridge() : nullptr;
}

void Renderer::setRadarShaderPath(const std::filesystem::path& vertPath,
                                  const std::filesystem::path& fragPath) {
    if (radarRenderer_) {
        radarRenderer_->setRadarShaderPath(vertPath, fragPath, core_->context().cameraPosition.y);
    }
}

glm::mat4 Renderer::getViewProjectionMatrix() const {
    return core_ ? core_->scene().getViewProjectionMatrix() : glm::mat4(1.0f);
}

glm::mat4 Renderer::getViewMatrix() const {
    return core_ ? core_->scene().getViewMatrix() : glm::mat4(1.0f);
}

glm::mat4 Renderer::getProjectionMatrix() const {
    return core_ ? core_->scene().getProjectionMatrix() : glm::mat4(1.0f);
}
