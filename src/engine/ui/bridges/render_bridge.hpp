#pragma once

#include "engine/graphics/texture_handle.hpp"
#include "engine/graphics/ui_render_target_bridge.hpp"

namespace ui {

class RenderBridge {
public:
    virtual ~RenderBridge() = default;
    virtual graphics::TextureHandle getRadarTexture() const = 0;
    virtual graphics_backend::UiRenderTargetBridge* getUiRenderTargetBridge() const { return nullptr; }
};

} // namespace ui
