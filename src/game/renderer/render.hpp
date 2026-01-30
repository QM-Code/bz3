#pragma once

#include "engine/ecs/components.hpp"
#include "engine/ecs/world.hpp"
#include "engine/renderer/render_context.hpp"
#include "engine/renderer/render_core.hpp"
#include "engine/renderer/scene_renderer.hpp"
#include "game/renderer/radar_renderer.hpp"
#include "core/types.hpp"
#include "ui/core/types.hpp"

#include <filesystem>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>


namespace platform {
class Window;
}
namespace graphics {
class ResourceRegistry;
}

class Render {
    friend class ClientEngine;

private:
    std::unique_ptr<engine::renderer::RenderCore> core_;
    platform::Window* window = nullptr;

    render_id nextId = 1;

    std::unique_ptr<game::renderer::RadarRenderer> radarRenderer_;
    std::unordered_map<render_id, ecs::EntityId> ecsEntities;

    ecs::World *ecsWorld = nullptr;
    graphics::ResourceRegistry *contextResources_ = nullptr;

    int lastFramebufferWidth = 0;
    int lastFramebufferHeight = 0;
    float lastAspect = 1.0f;

    Render(platform::Window &window);
    ~Render();

    void update();
    void resizeCallback(int width, int height);

    void registerEcsEntity(render_id id);
    ecs::Transform *getEcsTransform(render_id id);
    graphics::EntityId getEcsGraphicsEntity(render_id id) const;
    void setEcsRenderMesh(render_id id, const std::filesystem::path& modelPath);

public:
    render_id create();
    render_id create(std::string modelPath, bool addToRadar = true);
    void setModel(render_id id, const std::filesystem::path& modelPath, bool addToRadar = true);
    void setRadarCircleGraphic(render_id id, float radius = 1.0f);
    void setRadarFOVLinesAngle(float fovDegrees);
    void setEcsWorld(ecs::World *world);
    void setResourceRegistry(graphics::ResourceRegistry *resources);
    void setMainLayer(graphics::LayerId layer) { if (core_) { core_->context().mainLayer = layer; } }
    engine::renderer::RenderContext &mainContext() { return core_->context(); }
    const engine::renderer::RenderContext &mainContext() const { return core_->context(); }

    void destroy(render_id id);
    void setPosition(render_id id, const glm::vec3 &position);
    void setRotation(render_id id, const glm::quat &rotation);
    void setScale(render_id id, const glm::vec3 &scale);
    void setVisible(render_id id, bool visible);
    void setTransparency(render_id id, bool transparency);
    void setCameraPosition(const glm::vec3 &position);
    void setCameraRotation(const glm::quat &rotation);
    void setUiOverlayTexture(const ui::RenderOutput& output);
    void renderUiOverlay();
    void setBrightness(float brightness);
    void present();

    graphics::TextureHandle getRadarTexture() const;
    graphics_backend::UiRenderTargetBridge* getUiRenderTargetBridge() const;
    void setRadarShaderPath(const std::filesystem::path& vertPath,
                            const std::filesystem::path& fragPath);
    graphics::GraphicsDevice *getGraphicsDevice() const { return core_ ? &core_->device() : nullptr; }
    engine::renderer::RenderCore *getRenderCore() const { return core_.get(); }

    glm::mat4 getViewProjectionMatrix() const;
    glm::mat4 getViewMatrix() const;
    glm::mat4 getProjectionMatrix() const;
    glm::vec3 getCameraPosition() const;
    glm::vec3 getCameraForward() const;
};
