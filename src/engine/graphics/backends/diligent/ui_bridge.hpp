#pragma once

#include <cstdint>

namespace Diligent {
class IRenderDevice;
class IDeviceContext;
class ISwapChain;
class ITextureView;
}

namespace graphics_backend::diligent_ui {

struct Context {
    Diligent::IRenderDevice* device = nullptr;
    Diligent::IDeviceContext* context = nullptr;
    Diligent::ISwapChain* swapChain = nullptr;
    int framebufferWidth = 0;
    int framebufferHeight = 0;
};

void SetContext(Diligent::IRenderDevice* device,
                Diligent::IDeviceContext* context,
                Diligent::ISwapChain* swapChain,
                int framebufferWidth,
                int framebufferHeight);
void ClearContext();
Context GetContext();

uint64_t RegisterExternalTexture(Diligent::ITextureView* view);
void UnregisterExternalTexture(uint64_t token);
Diligent::ITextureView* ResolveExternalTexture(uint64_t token);

} // namespace graphics_backend::diligent_ui
