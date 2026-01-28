#include "engine/graphics/backend.hpp"

#include <spdlog/spdlog.h>

#if defined(BZ3_RENDER_BACKEND_FILAMENT)
#include "engine/graphics/backends/filament/backend.hpp"
#elif defined(BZ3_RENDER_BACKEND_DILIGENT)
#include "engine/graphics/backends/diligent/backend.hpp"
#elif defined(BZ3_RENDER_BACKEND_BGFX)
#include "engine/graphics/backends/bgfx/backend.hpp"
#else
#error "BZ3 render backend not set. Define BZ3_RENDER_BACKEND_FILAMENT, BZ3_RENDER_BACKEND_DILIGENT, or BZ3_RENDER_BACKEND_BGFX."
#endif

namespace graphics_backend {

std::unique_ptr<Backend> CreateGraphicsBackend(platform::Window& window) {
#if defined(BZ3_RENDER_BACKEND_FILAMENT)
    return std::make_unique<FilamentBackend>(window);
#elif defined(BZ3_RENDER_BACKEND_DILIGENT)
    return std::make_unique<DiligentBackend>(window);
#elif defined(BZ3_RENDER_BACKEND_BGFX)
    spdlog::info("Graphics: selecting bgfx backend");
    return std::make_unique<BgfxBackend>(window);
#else
    (void)window;
    return nullptr;
#endif
}

} // namespace graphics_backend
