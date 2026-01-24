#include "physics/backend.hpp"

#if defined(BZ3_PHYSICS_BACKEND_JOLT)
#include "physics/backends/jolt/physics_world_jolt.hpp"
#elif defined(BZ3_PHYSICS_BACKEND_BULLET)
#include "physics/backends/bullet/physics_world_bullet.hpp"
#else
#error "BZ3 physics backend not set. Define BZ3_PHYSICS_BACKEND_JOLT or BZ3_PHYSICS_BACKEND_BULLET."
#endif

namespace physics_backend {

std::unique_ptr<PhysicsWorldBackend> CreatePhysicsWorldBackend() {
#if defined(BZ3_PHYSICS_BACKEND_JOLT)
    return std::make_unique<PhysicsWorldJolt>();
#elif defined(BZ3_PHYSICS_BACKEND_BULLET)
    return std::make_unique<PhysicsWorldBullet>();
#endif
}

} // namespace physics_backend
