#include "renderer/renderer.hpp"
#include "renderer/radar_components.hpp"
#include "karma/graphics/ui_render_target_bridge.hpp"
#include "karma/graphics/resources.hpp"

#include "karma/platform/window.hpp"
#include "spdlog/spdlog.h"
#include <cmath>
#include <unordered_set>

namespace {

class RendererUiBridge final : public ui::UiRenderTargetBridge {
public:
    explicit RendererUiBridge(graphics_backend::UiRenderTargetBridge* bridge)
        : bridge_(bridge) {}

    void* toImGuiTextureId(const graphics::TextureHandle& texture) const override {
        return bridge_ ? bridge_->toImGuiTextureId(texture) : nullptr;
    }

    void rebuildImGuiFonts(ImFontAtlas* atlas) override {
        if (bridge_) {
            bridge_->rebuildImGuiFonts(atlas);
        }
    }

    void renderImGuiToTarget(ImDrawData* drawData) override {
        if (bridge_) {
            bridge_->renderImGuiToTarget(drawData);
        }
    }

    bool isImGuiReady() const override {
        return bridge_ && bridge_->isImGuiReady();
    }

    void ensureImGuiRenderTarget(int width, int height) override {
        if (bridge_) {
            bridge_->ensureImGuiRenderTarget(width, height);
        }
    }

    graphics::TextureHandle getImGuiRenderTarget() const override {
        return bridge_ ? bridge_->getImGuiRenderTarget() : graphics::TextureHandle{};
    }

private:
    graphics_backend::UiRenderTargetBridge* bridge_ = nullptr;
};

} // namespace

Renderer::Renderer(platform::Window &windowIn)
    : window(&windowIn) {
    core_ = std::make_unique<engine::renderer::RendererCore>(windowIn);
    radarRenderer_ = std::make_unique<game::renderer::RadarRenderer>(core_->device(), core_->scene());
    if (auto* backendBridge = core_->device().getUiRenderTargetBridge()) {
        uiRenderTargetBridge_ = std::make_unique<RendererUiBridge>(backendBridge);
    }

}

Renderer::~Renderer() = default;

void Renderer::renderRadar(const glm::vec3 &cameraPosition, const glm::quat &cameraRotation) {
    if (!core_) {
        return;
    }
    if (radarRenderer_) {
        const float fovDeg = core_->context().fov;
        const float halfVertRad = glm::radians(fovDeg * 0.5f);
        const float halfHorizRad = std::atan(std::tan(halfVertRad) * core_->context().aspect);
        radarRenderer_->setFovDegrees(glm::degrees(halfHorizRad * 2.0f));
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
    const auto &meshes = ecsWorld->all<ecs::MeshComponent>();
    const auto &transforms = ecsWorld->all<ecs::Transform>();
    if (radarTags.empty()) {
        if (!radarEcsEntities_.empty()) {
            for (const auto &entry : radarEcsEntities_) {
                radarRenderer_->destroy(entry.second.id);
            }
            radarEcsEntities_.clear();
        }
    } else {
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

    const auto &radarCircles = ecsWorld->all<game::renderer::RadarCircle>();
    if (radarCircles.empty()) {
        if (!radarEcsCircles_.empty()) {
            for (const auto &entry : radarEcsCircles_) {
                radarRenderer_->destroy(entry.second.id);
            }
            radarEcsCircles_.clear();
        }
        return;
    }

    std::unordered_set<ecs::EntityId> seenCircles;
    seenCircles.reserve(radarCircles.size());

    for (const auto &pair : radarCircles) {
        const ecs::EntityId entity = pair.first;
        const auto &circle = pair.second;
        if (!circle.enabled) {
            continue;
        }
        const auto transformIt = transforms.find(entity);
        if (transformIt == transforms.end()) {
            continue;
        }

        seenCircles.insert(entity);
        auto it = radarEcsCircles_.find(entity);
        if (it == radarEcsCircles_.end()) {
            const render_id id = nextId++;
            radarRenderer_->setRadarCircleGraphic(id, circle.radius);
            radarRenderer_->setPosition(id, transformIt->second.position);
            radarEcsCircles_.insert({entity, RadarEcsCircleEntry{id, circle.radius}});
        } else {
            if (std::abs(it->second.radius - circle.radius) > 0.0001f) {
                radarRenderer_->setRadarCircleGraphic(it->second.id, circle.radius);
                it->second.radius = circle.radius;
            }
            radarRenderer_->setPosition(it->second.id, transformIt->second.position);
        }
    }

    for (auto it = radarEcsCircles_.begin(); it != radarEcsCircles_.end(); ) {
        if (seenCircles.find(it->first) == seenCircles.end()) {
            radarRenderer_->destroy(it->second.id);
            it = radarEcsCircles_.erase(it);
        } else {
            ++it;
        }
    }
}

graphics::TextureHandle Renderer::getRadarTexture() const {
    return radarRenderer_ ? radarRenderer_->getRadarTexture() : graphics::TextureHandle{};
}

ui::UiRenderTargetBridge* Renderer::getUiRenderTargetBridge() const {
    return uiRenderTargetBridge_.get();
}

void Renderer::configureRadar(const game::renderer::RadarConfig& config) {
    if (radarRenderer_) {
        radarRenderer_->configure(config);
    }
}
