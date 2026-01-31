#include "karma/graphics/backends/diligent/ui_bridge.hpp"

#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>

#include <atomic>
#include <mutex>
#include <unordered_map>

namespace graphics_backend::diligent_ui {
namespace {
    std::mutex g_mutex;
    Context g_context{};
    std::unordered_map<uint64_t, Diligent::ITextureView*> g_textures;
    std::atomic<uint64_t> g_nextToken{1};
} // namespace

void SetContext(Diligent::IRenderDevice* device,
                Diligent::IDeviceContext* context,
                Diligent::ISwapChain* swapChain,
                int framebufferWidth,
                int framebufferHeight) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_context.device = device;
    g_context.context = context;
    g_context.swapChain = swapChain;
    g_context.framebufferWidth = framebufferWidth;
    g_context.framebufferHeight = framebufferHeight;
}

void ClearContext() {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_context = {};
    g_textures.clear();
    g_nextToken = 1;
}

Context GetContext() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_context;
}

uint64_t RegisterExternalTexture(Diligent::ITextureView* view) {
    if (!view) {
        return 0;
    }
    std::lock_guard<std::mutex> lock(g_mutex);
    const uint64_t token = g_nextToken++;
    g_textures[token] = view;
    return token;
}

void UnregisterExternalTexture(uint64_t token) {
    if (token == 0) {
        return;
    }
    std::lock_guard<std::mutex> lock(g_mutex);
    g_textures.erase(token);
}

Diligent::ITextureView* ResolveExternalTexture(uint64_t token) {
    if (token == 0) {
        return nullptr;
    }
    std::lock_guard<std::mutex> lock(g_mutex);
    auto it = g_textures.find(token);
    if (it == g_textures.end()) {
        return nullptr;
    }
    return it->second;
}

} // namespace graphics_backend::diligent_ui
