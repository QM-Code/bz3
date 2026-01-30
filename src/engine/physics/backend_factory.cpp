#include "physics/backend.hpp"

#if defined(KARMA_PHYSICS_BACKEND_JOLT)
#include "physics/backends/jolt/physics_world_jolt.hpp"
#elif defined(KARMA_PHYSICS_BACKEND_BULLET)
#include "physics/backends/bullet/physics_world_bullet.hpp"
#elif defined(KARMA_PHYSICS_BACKEND_PHYSX)
#include "physics/backends/physx/physics_world_physx.hpp"
#else
#error "KARMA physics backend not set. Define KARMA_PHYSICS_BACKEND_JOLT, KARMA_PHYSICS_BACKEND_BULLET, or KARMA_PHYSICS_BACKEND_PHYSX."
#endif

namespace physics_backend {

std::unique_ptr<PhysicsWorldBackend> CreatePhysicsWorldBackend() {
#if defined(KARMA_PHYSICS_BACKEND_JOLT)
    return std::make_unique<PhysicsWorldJolt>();
#elif defined(KARMA_PHYSICS_BACKEND_BULLET)
    return std::make_unique<PhysicsWorldBullet>();
#elif defined(KARMA_PHYSICS_BACKEND_PHYSX)
    return std::make_unique<PhysicsWorldPhysX>();
#endif
}

} // namespace physics_backend
