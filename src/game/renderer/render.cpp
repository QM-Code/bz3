#include "renderer/render.hpp"
#include "engine/graphics/resources.hpp"

#include "platform/window.hpp"
#include "spdlog/spdlog.h"

Render::Render(platform::Window &windowIn)
    : window(&windowIn) {
    core_ = std::make_unique<engine::renderer::RenderCore>(windowIn);
    radarRenderer_ = std::make_unique<game::renderer::RadarRenderer>(core_->device(), core_->scene());

}

Render::~Render() = default;

void Render::resizeCallback(int width, int height) {
    if (core_) {
        core_->scene().resize(width, height);
    }
}

void Render::update() {
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

void Render::setEcsWorld(ecs::World *world) {
    ecsWorld = world;
}

void Render::setResourceRegistry(graphics::ResourceRegistry *resources) {
    contextResources_ = resources;
}

void Render::registerEcsEntity(render_id id) {
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

ecs::Transform *Render::getEcsTransform(render_id id) {
    if (!ecsWorld) {
        return nullptr;
    }
    auto it = ecsEntities.find(id);
    if (it == ecsEntities.end()) {
        return nullptr;
    }
    return ecsWorld->get<ecs::Transform>(it->second);
}

graphics::EntityId Render::getEcsGraphicsEntity(render_id id) const {
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

void Render::setEcsRenderMesh(render_id id, const std::filesystem::path& modelPath) {
    if (!ecsWorld) {
        return;
    }
    if (!contextResources_) {
        spdlog::warn("Render: ResourceRegistry unavailable; ECS mesh not set for {}", modelPath.string());
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

render_id Render::create() {
    const render_id id = nextId++;
    registerEcsEntity(id);
    if (ecsWorld) {
        ecsWorld->set(ecsEntities[id], ecs::RenderLayer{core_->context().mainLayer});
    }
    return id;
}

render_id Render::create(std::string modelPath, bool addToRadar) {
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

void Render::setModel(render_id id, const std::filesystem::path& modelPath, bool addToRadar) {
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

void Render::setRadarCircleGraphic(render_id id, float radius) {
    if (radarRenderer_) {
        radarRenderer_->setRadarCircleGraphic(id, radius);
    }
}

void Render::setRadarFOVLinesAngle(float fovDegrees) {
    if (radarRenderer_) {
        radarRenderer_->setFovDegrees(fovDegrees);
    }
}

void Render::destroy(render_id id) {
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

void Render::setPosition(render_id id, const glm::vec3 &position) {
    if (radarRenderer_) {
        radarRenderer_->setPosition(id, position);
    }
    if (auto *transform = getEcsTransform(id)) {
        transform->position = position;
    }
}

void Render::setRotation(render_id id, const glm::quat &rotation) {
    if (radarRenderer_) {
        radarRenderer_->setRotation(id, rotation);
    }
    if (auto *transform = getEcsTransform(id)) {
        transform->rotation = rotation;
    }
}

void Render::setScale(render_id id, const glm::vec3 &scale) {
    if (radarRenderer_) {
        radarRenderer_->setScale(id, scale);
    }
    if (auto *transform = getEcsTransform(id)) {
        transform->scale = scale;
    }
}

void Render::setVisible(render_id id, bool visible) {
    if (const auto gfxEntity = getEcsGraphicsEntity(id); gfxEntity != graphics::kInvalidEntity) {
        core_->device().setVisible(gfxEntity, visible);
    }
    if (radarRenderer_) {
        radarRenderer_->setVisible(id, visible);
    }
}

void Render::setTransparency(render_id id, bool transparency) {
    if (const auto gfxEntity = getEcsGraphicsEntity(id); gfxEntity != graphics::kInvalidEntity) {
        core_->device().setTransparency(gfxEntity, transparency);
    }
}

void Render::setCameraPosition(const glm::vec3 &position) {
    core_->context().cameraPosition = position;
}

void Render::setCameraRotation(const glm::quat &rotation) {
    core_->context().cameraRotation = rotation;
}

graphics::TextureHandle Render::getRadarTexture() const {
    return radarRenderer_ ? radarRenderer_->getRadarTexture() : graphics::TextureHandle{};
}

graphics_backend::UiRenderTargetBridge* Render::getUiRenderTargetBridge() const {
    return core_ ? core_->device().getUiRenderTargetBridge() : nullptr;
}

void Render::setUiOverlayTexture(const ui::RenderOutput& output) {
    if (!core_) {
        return;
    }
    core_->scene().setUiOverlayTexture(output.texture, output.valid());
}

void Render::renderUiOverlay() {
    if (!core_) {
        return;
    }
    core_->scene().renderUiOverlay();
}

void Render::setBrightness(float brightness) {
    if (!core_) {
        return;
    }
    core_->scene().setBrightness(brightness);
}

void Render::present() {
    if (!core_) {
        return;
    }
    core_->scene().endFrame();
}

void Render::setRadarShaderPath(const std::filesystem::path& vertPath,
                                const std::filesystem::path& fragPath) {
    if (radarRenderer_) {
        radarRenderer_->setRadarShaderPath(vertPath, fragPath, core_->context().cameraPosition.y);
    }
}

glm::mat4 Render::getViewProjectionMatrix() const {
    return core_ ? core_->scene().getViewProjectionMatrix() : glm::mat4(1.0f);
}

glm::mat4 Render::getViewMatrix() const {
    return core_ ? core_->scene().getViewMatrix() : glm::mat4(1.0f);
}

glm::mat4 Render::getProjectionMatrix() const {
    return core_ ? core_->scene().getProjectionMatrix() : glm::mat4(1.0f);
}

glm::vec3 Render::getCameraPosition() const {
    return core_ ? core_->context().cameraPosition : glm::vec3(0.0f);
}

glm::vec3 Render::getCameraForward() const {
    return core_ ? core_->scene().getCameraForward() : glm::vec3(0.0f, 0.0f, -1.0f);
}
