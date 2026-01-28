#pragma once

#include <cstdint>

#include <imgui.h>

#include "engine/graphics/texture_handle.hpp"

namespace ui {

inline ImTextureID ToImGuiTextureId(const graphics::TextureHandle& texture) {
    const uint64_t token = texture.id;
    if (token == 0) {
        return static_cast<ImTextureID>(0);
    }
#if defined(BZ3_RENDER_BACKEND_BGFX)
    return static_cast<ImTextureID>(token);
#else
    return static_cast<ImTextureID>(token);
#endif
}

} // namespace ui
