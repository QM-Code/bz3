#include "renderer/renderer.hpp"
#include "karma/graphics/resources.hpp"

#include "karma/platform/window.hpp"
#include "spdlog/spdlog.h"

Renderer::Renderer(platform::Window &windowIn)
    : window(&windowIn) {
    core_ = std::make_unique<engine::renderer::RendererCore>(windowIn);
    radarRenderer_ = std::make_unique<game::renderer::RadarRenderer>(core_->device(), core_->scene());

}

Renderer::~Renderer() = default;

void Renderer::resizeCallback(int width, int height) {
    if (core_) {
        core_->scene().resize(width, height);
    }
}

void Renderer::update() {
    if (!core_) {
        return;
    }

    int width = 0;
    int height = 0;
    if (window) {
        window->getFramebufferSize(width, height);
    }
    if (height <= 0) {
        height = 1;
    }
    if (width <= 0) {
        width = 1;
    }

    if (width != lastFramebufferWidth || height != lastFramebufferHeight) {
        lastFramebufferWidth = width;
        lastFramebufferHeight = height;
        lastAspect = static_cast<float>(width) / static_cast<float>(height);
        core_->scene().resize(width, height);
    }

    core_->scene().beginFrame();

    if (radarRenderer_) {
        radarRenderer_->render(core_->context().cameraPosition, core_->context().cameraRotation);
    }

    // Render main layer to screen
    core_->context().aspect = lastAspect;
    core_->scene().renderMain(core_->context());

}

void Renderer::setEcsWorld(ecs::World *world) {
    ecsWorld = world;
}

void Renderer::setResourceRegistry(graphics::ResourceRegistry *resources) {
    contextResources_ = resources;
}

void Renderer::registerEcsEntity(render_id id) {
    if (!ecsWorld) {
        return;
    }
    if (ecsEntities.find(id) != ecsEntities.end()) {
        return;
    }
    const ecs::EntityId entity = ecsWorld->createEntity();
    ecsEntities[id] = entity;
    ecsWorld->set(entity, ecs::Transform{});
}

ecs::Transform *Renderer::getEcsTransform(render_id id) {
    if (!ecsWorld) {
        return nullptr;
    }
    auto it = ecsEntities.find(id);
    if (it == ecsEntities.end()) {
        return nullptr;
    }
    return ecsWorld->get<ecs::Transform>(it->second);
}

graphics::EntityId Renderer::getEcsGraphicsEntity(render_id id) const {
    if (!ecsWorld) {
        return graphics::kInvalidEntity;
    }
    auto it = ecsEntities.find(id);
    if (it == ecsEntities.end()) {
        return graphics::kInvalidEntity;
    }
    const ecs::RenderEntity *renderEntity = ecsWorld->get<ecs::RenderEntity>(it->second);
    return renderEntity ? renderEntity->entityId : graphics::kInvalidEntity;
}

void Renderer::setEcsRenderMesh(render_id id, const std::filesystem::path& modelPath) {
    if (!ecsWorld) {
        return;
    }
    if (!contextResources_) {
        spdlog::warn("Renderer: ResourceRegistry unavailable; ECS mesh not set for {}", modelPath.string());
        return;
    }
    auto it = ecsEntities.find(id);
    if (it == ecsEntities.end()) {
        return;
    }
    graphics::LayerId desiredLayer = core_ ? core_->context().mainLayer : 0;
    if (const auto *layer = ecsWorld->get<ecs::RenderLayer>(it->second)) {
        desiredLayer = layer->layer;
    }
    if (core_) {
        const graphics::EntityId gfxEntity = core_->device().createModelEntity(
            modelPath,
            desiredLayer,
            graphics::kInvalidMaterial
        );
        if (gfxEntity != graphics::kInvalidEntity) {
            ecsWorld->set(it->second, ecs::RenderEntity{gfxEntity});
            ecsWorld->remove<ecs::RenderMesh>(it->second);
            return;
        }
    }
    const graphics::MeshId meshId = contextResources_->loadMesh(modelPath);
    if (meshId == graphics::kInvalidMesh) {
        return;
    }
    ecsWorld->set(it->second, ecs::RenderMesh{meshId});
}

render_id Renderer::create() {
    const render_id id = nextId++;
    registerEcsEntity(id);
    if (ecsWorld) {
        ecsWorld->set(ecsEntities[id], ecs::RenderLayer{core_->context().mainLayer});
    }
    return id;
}

render_id Renderer::create(std::string modelPath, bool addToRadar) {
    const render_id id = nextId++;
    registerEcsEntity(id);
    setEcsRenderMesh(id, modelPath);
    if (ecsWorld) {
        ecsWorld->set(ecsEntities[id], ecs::RenderLayer{core_->context().mainLayer});
        if (contextResources_) {
            ecsWorld->set(ecsEntities[id], ecs::Material{contextResources_->getDefaultMaterial()});
        }
    }

    if (radarRenderer_) {
        radarRenderer_->setModel(id, modelPath, addToRadar);
    }

    return id;
}

void Renderer::setModel(render_id id, const std::filesystem::path& modelPath, bool addToRadar) {
    registerEcsEntity(id);
    setEcsRenderMesh(id, modelPath);
    if (ecsWorld) {
        ecsWorld->set(ecsEntities[id], ecs::RenderLayer{core_->context().mainLayer});
        if (contextResources_) {
            ecsWorld->set(ecsEntities[id], ecs::Material{contextResources_->getDefaultMaterial()});
        }
    }

    if (radarRenderer_) {
        radarRenderer_->setModel(id, modelPath, addToRadar);
    }
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
    if (ecsWorld) {
        if (auto it = ecsEntities.find(id); it != ecsEntities.end()) {
            ecsWorld->destroyEntity(it->second);
        }
    }
    ecsEntities.erase(id);
}

void Renderer::setPosition(render_id id, const glm::vec3 &position) {
    if (radarRenderer_) {
        radarRenderer_->setPosition(id, position);
    }
    if (auto *transform = getEcsTransform(id)) {
        transform->position = position;
    }
}

void Renderer::setRotation(render_id id, const glm::quat &rotation) {
    if (radarRenderer_) {
        radarRenderer_->setRotation(id, rotation);
    }
    if (auto *transform = getEcsTransform(id)) {
        transform->rotation = rotation;
    }
}

void Renderer::setScale(render_id id, const glm::vec3 &scale) {
    if (radarRenderer_) {
        radarRenderer_->setScale(id, scale);
    }
    if (auto *transform = getEcsTransform(id)) {
        transform->scale = scale;
    }
}

void Renderer::setVisible(render_id id, bool visible) {
    if (const auto gfxEntity = getEcsGraphicsEntity(id); gfxEntity != graphics::kInvalidEntity) {
        core_->device().setVisible(gfxEntity, visible);
    }
    if (radarRenderer_) {
        radarRenderer_->setVisible(id, visible);
    }
}

void Renderer::setTransparency(render_id id, bool transparency) {
    if (const auto gfxEntity = getEcsGraphicsEntity(id); gfxEntity != graphics::kInvalidEntity) {
        core_->device().setTransparency(gfxEntity, transparency);
    }
}

void Renderer::setCameraPosition(const glm::vec3 &position) {
    core_->context().cameraPosition = position;
}

void Renderer::setCameraRotation(const glm::quat &rotation) {
    core_->context().cameraRotation = rotation;
}

graphics::TextureHandle Renderer::getRadarTexture() const {
    return radarRenderer_ ? radarRenderer_->getRadarTexture() : graphics::TextureHandle{};
}

graphics_backend::UiRenderTargetBridge* Renderer::getUiRenderTargetBridge() const {
    return core_ ? core_->device().getUiRenderTargetBridge() : nullptr;
}

void Renderer::setUiOverlayTexture(const ui::RenderOutput& output) {
    if (!core_) {
        return;
    }
    core_->scene().setUiOverlayTexture(output.texture, output.valid());
}

void Renderer::renderUiOverlay() {
    if (!core_) {
        return;
    }
    core_->scene().renderUiOverlay();
}

void Renderer::setBrightness(float brightness) {
    if (!core_) {
        return;
    }
    core_->scene().setBrightness(brightness);
}

void Renderer::present() {
    if (!core_) {
        return;
    }
    core_->scene().endFrame();
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

glm::vec3 Renderer::getCameraPosition() const {
    return core_ ? core_->context().cameraPosition : glm::vec3(0.0f);
}

glm::vec3 Renderer::getCameraForward() const {
    return core_ ? core_->scene().getCameraForward() : glm::vec3(0.0f, 0.0f, -1.0f);
}
