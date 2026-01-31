#include "karma/graphics/backends/forge/ui_bridge.hpp"

#include <atomic>
#include <mutex>
#include <unordered_map>

namespace graphics_backend::forge_ui {
namespace {
    std::mutex g_mutex;
    Context g_context{};
    std::unordered_map<uint64_t, void*> g_textures;
    std::atomic<uint64_t> g_nextToken{1};
} // namespace

void SetContext(void* renderer,
                void* graphicsQueue,
                int framebufferWidth,
                int framebufferHeight,
                uint32_t colorFormat) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_context.renderer = renderer;
    g_context.graphicsQueue = graphicsQueue;
    g_context.framebufferWidth = framebufferWidth;
    g_context.framebufferHeight = framebufferHeight;
    g_context.colorFormat = colorFormat;
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

uint64_t RegisterExternalTexture(void* texture) {
    if (!texture) {
        return 0;
    }
    std::lock_guard<std::mutex> lock(g_mutex);
    const uint64_t token = g_nextToken++;
    g_textures[token] = texture;
    return token;
}

void UnregisterExternalTexture(uint64_t token) {
    if (token == 0) {
        return;
    }
    std::lock_guard<std::mutex> lock(g_mutex);
    g_textures.erase(token);
}

void* ResolveExternalTexture(uint64_t token) {
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

} // namespace graphics_backend::forge_ui
