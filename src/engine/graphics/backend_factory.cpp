#include "engine/graphics/backend.hpp"

#if defined(BZ3_RENDER_BACKEND_THREEPP)
#include "engine/graphics/backends/threepp/backend.hpp"
#elif defined(BZ3_RENDER_BACKEND_FILAMENT)
#include "engine/graphics/backends/filament/backend.hpp"
#else
#error "BZ3 render backend not set. Define BZ3_RENDER_BACKEND_THREEPP or BZ3_RENDER_BACKEND_FILAMENT."
#endif

namespace graphics_backend {

std::unique_ptr<Backend> CreateGraphicsBackend(platform::Window& window) {
#if defined(BZ3_RENDER_BACKEND_THREEPP)
    return std::make_unique<ThreeppBackend>(window);
#elif defined(BZ3_RENDER_BACKEND_FILAMENT)
    return std::make_unique<FilamentBackend>(window);
#else
    (void)window;
    return nullptr;
#endif
}

} // namespace graphics_backend
