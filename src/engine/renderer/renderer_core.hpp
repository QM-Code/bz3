#pragma once

#include "karma/graphics/device.hpp"
#include "karma/renderer/renderer_context.hpp"
#include "karma/renderer/scene_renderer.hpp"
#include "karma/renderer/renderer_context.hpp"
#include <memory>

namespace engine::renderer {

class RendererCore {
public:
    explicit RendererCore(platform::Window &window)
        : device_(std::make_unique<graphics::GraphicsDevice>(window)),
          scene_(std::make_unique<SceneRenderer>(*device_)) {}

    graphics::GraphicsDevice &device() { return *device_; }
    const graphics::GraphicsDevice &device() const { return *device_; }

    SceneRenderer &scene() { return *scene_; }
    const SceneRenderer &scene() const { return *scene_; }

    RendererContext &context() { return context_; }
    const RendererContext &context() const { return context_; }

private:
    std::unique_ptr<graphics::GraphicsDevice> device_;
    std::unique_ptr<SceneRenderer> scene_;
    RendererContext context_{};
};

} // namespace engine::renderer
