#pragma once

#include "engine/graphics/texture_handle.hpp"
#include "engine/graphics/ui_bridge.hpp"

namespace ui {

class RenderBridge {
public:
    virtual ~RenderBridge() = default;
    virtual graphics::TextureHandle getRadarTexture() const = 0;
    virtual graphics_backend::UiBridge* getUiBridge() const = 0;
};

} // namespace ui
