#pragma once

#include "karma/ecs/components.hpp"
#include "karma/ecs/world.hpp"
#include "karma/renderer/renderer_context.hpp"
#include "karma/renderer/renderer_core.hpp"
#include "karma/renderer/scene_renderer.hpp"
#include "renderer/radar_renderer.hpp"
#include "ui/bridges/ui_render_target_bridge.hpp"
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
    std::unique_ptr<ui::UiRenderTargetBridge> uiRenderTargetBridge_;
    struct RadarEcsEntry {
        render_id id = 0;
        std::string mesh_key{};
    };
    std::unordered_map<ecs::EntityId, RadarEcsEntry> radarEcsEntities_;
    struct RadarEcsCircleEntry {
        render_id id = 0;
        float radius = 1.0f;
    };
    std::unordered_map<ecs::EntityId, RadarEcsCircleEntry> radarEcsCircles_;

    ecs::World *ecsWorld = nullptr;
    bool ecsRadarSyncEnabled = true;


    Renderer(platform::Window &window);
    ~Renderer();

    void renderRadar(const glm::vec3 &cameraPosition, const glm::quat &cameraRotation);

    void syncEcsRadar();

public:
    void setEcsWorld(ecs::World *world);
    void setMainLayer(graphics::LayerId layer) { if (core_) { core_->context().mainLayer = layer; } }
    graphics::TextureHandle getRadarTexture() const;
    ui::UiRenderTargetBridge* getUiRenderTargetBridge() const;
    void configureRadar(const game::renderer::RadarConfig& config);
    engine::renderer::RendererCore *getRendererCore() const { return core_.get(); }

};
