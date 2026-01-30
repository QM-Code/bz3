#pragma once

#include "engine/graphics/device.hpp"
#include "engine/renderer/render_context.hpp"
#include "engine/renderer/scene_renderer.hpp"
#include "engine/renderer/render_context.hpp"
#include <memory>

namespace engine::renderer {

class RenderCore {
public:
    explicit RenderCore(platform::Window &window)
        : device_(std::make_unique<graphics::GraphicsDevice>(window)),
          scene_(std::make_unique<SceneRenderer>(*device_)) {}

    graphics::GraphicsDevice &device() { return *device_; }
    const graphics::GraphicsDevice &device() const { return *device_; }

    SceneRenderer &scene() { return *scene_; }
    const SceneRenderer &scene() const { return *scene_; }

    RenderContext &context() { return context_; }
    const RenderContext &context() const { return context_; }

private:
    std::unique_ptr<graphics::GraphicsDevice> device_;
    std::unique_ptr<SceneRenderer> scene_;
    RenderContext context_{};
};

} // namespace engine::renderer
