#pragma once

#include <cstdint>

namespace graphics_backend::forge_ui {

struct Context {
    void* renderer = nullptr;
    void* graphicsQueue = nullptr;
    int framebufferWidth = 0;
    int framebufferHeight = 0;
    uint32_t colorFormat = 0;
};

void SetContext(void* renderer,
                void* graphicsQueue,
                int framebufferWidth,
                int framebufferHeight,
                uint32_t colorFormat);
void ClearContext();
Context GetContext();

uint64_t RegisterExternalTexture(void* texture);
void UnregisterExternalTexture(uint64_t token);
void* ResolveExternalTexture(uint64_t token);

} // namespace graphics_backend::forge_ui
