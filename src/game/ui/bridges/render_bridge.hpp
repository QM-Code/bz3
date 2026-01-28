#pragma once

#include "engine/graphics/texture_handle.hpp"

namespace ui {

class RenderBridge {
public:
    virtual ~RenderBridge() = default;
    virtual graphics::TextureHandle getRadarTexture() const = 0;
};

} // namespace ui
