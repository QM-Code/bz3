#pragma once

#include "engine/graphics/ui_render_target_bridge.hpp"

#include <cstdint>
#include <cstddef>

#include "Common_3/Graphics/Interfaces/IGraphics.h"

struct CmdPool;
struct Cmd;
struct Fence;

namespace graphics_backend {

class ForgeRenderer final : public UiRenderTargetBridge {
public:
    ForgeRenderer();
    ~ForgeRenderer() override;

    void* toImGuiTextureId(const graphics::TextureHandle& texture) const override;
    void rebuildImGuiFonts(ImFontAtlas* atlas) override;
    void renderImGuiToTarget(ImDrawData* drawData) override;
    bool isImGuiReady() const override;
    void ensureImGuiRenderTarget(int width, int height) override;
    graphics::TextureHandle getImGuiRenderTarget() const override;

private:
    bool ensureReady();
    void ensurePipeline();
    void ensureBuffers(std::size_t vertexBytes, std::size_t indexBytes);
    void destroyResources();
    uint32_t nextDescriptorSetIndex();

    Renderer* renderer_ = nullptr;
    Queue* queue_ = nullptr;
    CmdPool* cmdPool_ = nullptr;
    Cmd* cmd_ = nullptr;
    Fence* fence_ = nullptr;

    RenderTarget* uiTarget_ = nullptr;
    Texture* fontTexture_ = nullptr;
    Sampler* sampler_ = nullptr;
    Shader* shader_ = nullptr;
    Pipeline* pipeline_ = nullptr;
    DescriptorSet* descriptorSet_ = nullptr;
    Buffer* vertexBuffer_ = nullptr;
    Buffer* indexBuffer_ = nullptr;
    Buffer* uniformBuffer_ = nullptr;
    std::size_t vertexBufferSize_ = 0;
    std::size_t indexBufferSize_ = 0;
    uint32_t colorFormat_ = 0;
    Descriptor descriptors_[3]{};
    uint32_t descriptorSetCursor_ = 0;

    uint64_t fontToken_ = 0;
    uint64_t uiToken_ = 0;
    int uiWidth_ = 0;
    int uiHeight_ = 0;
    bool ready_ = false;
    bool fontsReady_ = false;
};

} // namespace graphics_backend
