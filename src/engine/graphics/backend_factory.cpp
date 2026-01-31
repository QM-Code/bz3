#include "karma/graphics/backend.hpp"

#include <spdlog/spdlog.h>

#if defined(KARMA_RENDER_BACKEND_DILIGENT)
#include "karma/graphics/backends/diligent/backend.hpp"
#elif defined(KARMA_RENDER_BACKEND_BGFX)
#include "karma/graphics/backends/bgfx/backend.hpp"
#elif defined(KARMA_RENDER_BACKEND_FORGE)
#include "karma/graphics/backends/forge/backend.hpp"
#else
#error "KARMA render backend not set. Define KARMA_RENDER_BACKEND_DILIGENT, KARMA_RENDER_BACKEND_BGFX, or KARMA_RENDER_BACKEND_FORGE."
#endif

namespace graphics_backend {

std::unique_ptr<Backend> CreateGraphicsBackend(platform::Window& window) {
#if defined(KARMA_RENDER_BACKEND_DILIGENT)
    return std::make_unique<DiligentBackend>(window);
#elif defined(KARMA_RENDER_BACKEND_BGFX)
    spdlog::info("Graphics: selecting bgfx backend");
    return std::make_unique<BgfxBackend>(window);
#elif defined(KARMA_RENDER_BACKEND_FORGE)
    spdlog::info("Graphics: selecting Forge backend");
    return std::make_unique<ForgeBackend>(window);
#else
    (void)window;
    return nullptr;
#endif
}

} // namespace graphics_backend
