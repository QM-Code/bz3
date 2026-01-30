#pragma once

#include "engine/app/game_interface.hpp"
#include "engine/ecs/system_graph.hpp"
#include "engine/ecs/world.hpp"
#include "engine/ecs/systems/render_system.hpp"
#include "engine/graphics/resources.hpp"
#include "engine/renderer/render_context.hpp"
#include <memory>

namespace graphics {
class GraphicsDevice;
}
class Input;
class Audio;
class PhysicsWorld;
namespace engine::renderer {
class RenderCore;
struct RenderContext;
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
    engine::renderer::RenderContext renderContext{};
    engine::renderer::RenderCore *renderCore = nullptr;
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
    ecs::RenderSystem renderSystem_{};
    std::unique_ptr<graphics::ResourceRegistry> resources_{};
};
} // namespace karma::app
