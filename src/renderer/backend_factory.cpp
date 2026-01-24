#include "renderer/backend.hpp"

#if defined(BZ3_RENDER_BACKEND_THREEPP)
#include "renderer/backends/threepp/backend.hpp"
#else
#error "BZ3 render backend not set. Define BZ3_RENDER_BACKEND_THREEPP."
#endif

namespace render_backend {

std::unique_ptr<Backend> CreateRenderBackend(platform::Window& window) {
#if defined(BZ3_RENDER_BACKEND_THREEPP)
    return std::make_unique<ThreeppBackend>(window);
#endif
}

} // namespace render_backend
