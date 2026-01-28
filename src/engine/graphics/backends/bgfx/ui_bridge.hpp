#pragma once

#include "engine/graphics/ui_bridge.hpp"

#include <bgfx/bgfx.h>
#include <cstdint>
#include <filesystem>
#include <vector>


namespace graphics_backend {

class BgfxUiBridge final : public UiBridge {
public:
    BgfxUiBridge();
    ~BgfxUiBridge() override;

    void* toImGuiTextureId(const graphics::TextureHandle& texture) const override;
    void rebuildImGuiFonts(ImFontAtlas* atlas) override;
    void renderImGuiDrawData(ImDrawData* drawData) override;
    bool isImGuiReady() const override;

private:
    struct ImGuiVertex {
        float x;
        float y;
        float u;
        float v;
        uint32_t abgr;
    };

    bgfx::ProgramHandle program_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle sampler_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle scaleBias_ = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle fontTexture_ = BGFX_INVALID_HANDLE;
    bgfx::VertexLayout layout_{};
    bool ready_ = false;
    bool fontsReady_ = false;

    void destroyResources();
    std::vector<uint8_t> readFileBytes(const std::filesystem::path& path) const;
    static uint16_t toTextureHandle(uint64_t textureId);
};

} // namespace graphics_backend
