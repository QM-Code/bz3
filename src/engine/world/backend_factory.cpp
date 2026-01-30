#include "world/backend.hpp"

#if defined(KARMA_WORLD_BACKEND_FS)
#include "world/backends/fs/backend.hpp"
#else
#error "BZ3 world backend not set. Define KARMA_WORLD_BACKEND_FS."
#endif

namespace world_backend {

std::unique_ptr<Backend> CreateWorldBackend() {
#if defined(KARMA_WORLD_BACKEND_FS)
    return std::make_unique<FsWorldBackend>();
#endif
}

} // namespace world_backend
