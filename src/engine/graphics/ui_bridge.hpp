#pragma once

#include "engine/graphics/texture_handle.hpp"

struct ImDrawData;
struct ImFontAtlas;

namespace graphics_backend {

class UiBridge {
public:
    virtual ~UiBridge() = default;

    virtual void* toImGuiTextureId(const graphics::TextureHandle& texture) const = 0;
    virtual void rebuildImGuiFonts(ImFontAtlas* atlas) = 0;
    virtual void renderImGuiDrawData(ImDrawData* drawData) = 0;
    virtual bool isImGuiReady() const = 0;
};

} // namespace graphics_backend
