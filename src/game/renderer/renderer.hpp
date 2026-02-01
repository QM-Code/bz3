#pragma once

#include "karma/ecs/components.hpp"
#include "karma/ecs/world.hpp"
#include "karma/renderer/renderer_context.hpp"
#include "karma/renderer/renderer_core.hpp"
#include "karma/renderer/scene_renderer.hpp"
#include "renderer/radar_renderer.hpp"
#include "karma/core/types.hpp"

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
}

class Renderer {
    friend class ClientEngine;

private:
    std::unique_ptr<engine::renderer::RendererCore> core_;
    platform::Window* window = nullptr;

    render_id nextId = 1;

    std::unique_ptr<game::renderer::RadarRenderer> radarRenderer_;
    struct RadarEcsEntry {
        render_id id = 0;
        std::string mesh_key{};
    };
    std::unordered_map<ecs::EntityId, RadarEcsEntry> radarEcsEntities_;

    ecs::World *ecsWorld = nullptr;
    bool ecsRadarSyncEnabled = false;


    Renderer(platform::Window &window);
    ~Renderer();

    void renderRadar(const glm::vec3 &cameraPosition, const glm::quat &cameraRotation);

    void syncEcsRadar();

public:
    render_id createRadarId();
    void setRadarCircleGraphic(render_id id, float radius = 1.0f);
    void setRadarFOVLinesAngle(float fovDegrees);
    void setEcsWorld(ecs::World *world);
    void setEcsRadarSyncEnabled(bool enabled) { ecsRadarSyncEnabled = enabled; }
    void setMainLayer(graphics::LayerId layer) { if (core_) { core_->context().mainLayer = layer; } }
    engine::renderer::RendererContext &mainContext() { return core_->context(); }
    const engine::renderer::RendererContext &mainContext() const { return core_->context(); }

    void destroy(render_id id);
    void setPosition(render_id id, const glm::vec3 &position);
    void setRotation(render_id id, const glm::quat &rotation);
    void setScale(render_id id, const glm::vec3 &scale);
    void setVisible(render_id id, bool visible);
    void setTransparency(render_id id, bool transparency);

    graphics::TextureHandle getRadarTexture() const;
    graphics_backend::UiRenderTargetBridge* getUiRenderTargetBridge() const;
    void setRadarShaderPath(const std::filesystem::path& vertPath,
                            const std::filesystem::path& fragPath);
    graphics::GraphicsDevice *getGraphicsDevice() const { return core_ ? &core_->device() : nullptr; }
    engine::renderer::RendererCore *getRendererCore() const { return core_.get(); }

    glm::mat4 getViewProjectionMatrix() const;
    glm::mat4 getViewMatrix() const;
    glm::mat4 getProjectionMatrix() const;
};
