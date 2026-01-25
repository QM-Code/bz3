#include "engine/graphics/backend.hpp"

#if defined(BZ3_RENDER_BACKEND_THREEPP)
#include "engine/graphics/backends/threepp/backend.hpp"
#else
#error "BZ3 render backend not set. Define BZ3_RENDER_BACKEND_THREEPP."
#endif

namespace graphics_backend {

std::unique_ptr<Backend> CreateGraphicsBackend(platform::Window& window) {
#if defined(BZ3_RENDER_BACKEND_THREEPP)
    return std::make_unique<ThreeppBackend>(window);
#else
    (void)window;
    return nullptr;
#endif
}

} // namespace graphics_backend
