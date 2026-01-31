#pragma once

#include "karma/app/game_interface.hpp"
#include "karma/ecs/system_graph.hpp"
#include "karma/ecs/world.hpp"
#include "karma/ecs/systems/renderer_system.hpp"
#include "karma/graphics/resources.hpp"
#include "karma/renderer/renderer_context.hpp"
#include <memory>

namespace graphics {
class GraphicsDevice;
}
class Input;
class Audio;
class PhysicsWorld;
namespace engine::renderer {
class RendererCore;
struct RendererContext;
}
namespace ui {
class Overlay;
}
namespace ecs {
class World;
}
namespace platform {
class Window;
}

namespace karma::app {
struct EngineContext {
    platform::Window *window = nullptr;
    graphics::GraphicsDevice *graphics = nullptr;
    Input *input = nullptr;
    Audio *audio = nullptr;
    PhysicsWorld *physics = nullptr;
    ui::Overlay *overlay = nullptr;
    ecs::World *ecsWorld = nullptr;
    graphics::ResourceRegistry *resources = nullptr;
    graphics::MaterialId defaultMaterial = graphics::kInvalidMaterial;
    engine::renderer::RendererContext rendererContext{};
    engine::renderer::RendererCore *rendererCore = nullptr;
};

class EngineApp {
public:
    EngineApp();
    ~EngineApp();

    void setGame(GameInterface *game);
    EngineContext &context();
    const EngineContext &context() const;
    int run();

private:
    GameInterface *game_ = nullptr;
    EngineContext context_{};
    ecs::World ecsWorld_{};
    ecs::SystemGraph systemGraph_{};
    ecs::RendererSystem rendererSystem_{};
    std::unique_ptr<graphics::ResourceRegistry> resources_{};
};
} // namespace karma::app
