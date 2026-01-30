#pragma once

#include <bgfx/bgfx.h>
#include <cstdint>

namespace graphics_backend::bgfx_utils {

inline uint16_t ToBgfxTextureHandle(uint64_t textureId) {
    if (textureId == 0) {
        return bgfx::kInvalidHandle;
    }
    return static_cast<uint16_t>(textureId - 1);
}

} // namespace graphics_backend::bgfx_utils
