#pragma once

#include <cstdint>

namespace graphics {

enum class TextureFormat : uint8_t {
    RGBA8_UNORM,
    BGRA8_UNORM,
    RGBA8_SRGB,
    BGRA8_SRGB,
};

struct TextureHandle {
    uint64_t id = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    TextureFormat format = TextureFormat::RGBA8_UNORM;

    constexpr bool valid() const {
        return id != 0 && width > 0 && height > 0;
    }
};

} // namespace graphics
