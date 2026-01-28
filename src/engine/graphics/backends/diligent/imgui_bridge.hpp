#pragma once

#include "engine/graphics/ui_bridge.hpp"

#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>

#include <cstdint>
#include <memory>

namespace Diligent {
class IBuffer;
class IShaderResourceBinding;
class IPipelineState;
class ITextureView;
}

namespace graphics_backend {

class DiligentImGuiBridge final : public UiBridge {
public:
    DiligentImGuiBridge();
    ~DiligentImGuiBridge() override;

    void* toImGuiTextureId(const graphics::TextureHandle& texture) const override;
    void rebuildImGuiFonts(ImFontAtlas* atlas) override;
    void renderImGuiDrawData(ImDrawData* drawData) override;
    bool isImGuiReady() const override;

private:
    void ensurePipeline();
    void ensureBuffers(std::size_t vertexBytes, std::size_t indexBytes);

    Diligent::RefCntAutoPtr<Diligent::IPipelineState> pipeline_;
    Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> shaderBinding_;
    Diligent::RefCntAutoPtr<Diligent::IBuffer> vertexBuffer_;
    Diligent::RefCntAutoPtr<Diligent::IBuffer> indexBuffer_;
    Diligent::RefCntAutoPtr<Diligent::IBuffer> constantBuffer_;
    std::size_t vertexBufferSize_ = 0;
    std::size_t indexBufferSize_ = 0;
    Diligent::RefCntAutoPtr<Diligent::ITextureView> fontSrv_;
    uint64_t fontToken_ = 0;
    bool ready_ = false;
};

} // namespace graphics_backend
