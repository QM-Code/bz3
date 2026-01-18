#include "engine/physics/physics_world.hpp"
#include "engine/physics/player_controller.hpp"
#include <Jolt/Jolt.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Body/BodyLockInterface.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>
#include <cstdarg>
#include <cstdio>
#include <glm/gtc/quaternion.hpp>
#include <spdlog/spdlog.h>
#include <thread>
#include "engine/physics/static_body.hpp"
#include "engine/mesh_loader.hpp"

namespace {
using namespace JPH;

constexpr uint32 MAX_BODIES = 4096;
constexpr uint32 NUM_BODY_MUTEXES = 0; // let Jolt decide
constexpr uint32 MAX_BODY_PAIRS = 65536;
constexpr uint32 MAX_CONTACT_CONSTRAINTS = 8192;

using PhysicsLayers::Moving;
using PhysicsLayers::NonMoving;

// Broadphase layers are small integers; store as JPH::BroadPhaseLayer values directly.
static constexpr BroadPhaseLayer BP_NON_MOVING(0);
static constexpr BroadPhaseLayer BP_MOVING(1);
static constexpr uint BP_LAYER_COUNT = 2;

class BPLayerInterfaceImpl final : public BroadPhaseLayerInterface {
public:
    BPLayerInterfaceImpl() {
        mObjectToBroadPhase[NonMoving] = BP_NON_MOVING;
        mObjectToBroadPhase[Moving] = BP_MOVING;
    }

    uint GetNumBroadPhaseLayers() const override { return BP_LAYER_COUNT; }
    BroadPhaseLayer GetBroadPhaseLayer(ObjectLayer layer) const override {
        return mObjectToBroadPhase[static_cast<size_t>(layer)];
    }

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    const char* GetBroadPhaseLayerName(BroadPhaseLayer layer) const override {
        if (layer == BP_NON_MOVING) return "NonMoving";
        if (layer == BP_MOVING) return "Moving";
        return "Unknown";
    }
#endif

private:
    BroadPhaseLayer mObjectToBroadPhase[2];
};

class ObjectLayerPairFilterImpl final : public ObjectLayerPairFilter {
public:
    bool ShouldCollide(ObjectLayer a, ObjectLayer b) const override {
        return true; // allow all combinations
    }
};

class ObjectVsBroadPhaseLayerFilterImpl final : public ObjectVsBroadPhaseLayerFilter {
public:
    bool ShouldCollide(ObjectLayer layer, BroadPhaseLayer bplayer) const override {
        JPH_UNUSED(layer);
        return bplayer == BP_MOVING || bplayer == BP_NON_MOVING;
    }
};

inline Vec3 toJph(const glm::vec3& v) { return Vec3(v.x, v.y, v.z); }
inline Quat toJph(const glm::quat& q) { return Quat(q.x, q.y, q.z, q.w); }
inline glm::vec3 toGlm(const Vec3& v) { return glm::vec3(v.GetX(), v.GetY(), v.GetZ()); }
inline glm::quat toGlm(const Quat& q) { return glm::quat(q.GetW(), q.GetX(), q.GetY(), q.GetZ()); }

void JoltTrace(const char* fmt, ...) {
    va_list list;
    va_start(list, fmt);
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), fmt, list);
    va_end(list);
    spdlog::trace("Jolt: {}", buffer);
}

void initJoltOnce() {
    static bool initialized = false;
    if (initialized) return;

    JPH::RegisterDefaultAllocator();
    JPH::Trace = &JoltTrace;
    JPH_IF_ENABLE_ASSERTS(JPH::AssertFailed = [](const char* expr, const char* msg, const char* file, uint line) {
        spdlog::error("Jolt assert failed: {} {} ({}:{})", expr, msg ? msg : "", file, line);
        return true;
    });

    JPH::Factory::sInstance = new JPH::Factory();
    JPH::RegisterTypes();
    initialized = true;
}
} // namespace

PhysicsWorld::PhysicsWorld() {
    initJoltOnce();

    tempAllocator_ = std::make_unique<TempAllocatorImpl>(32u * 1024u * 1024u);
    jobSystem_ = std::make_unique<JobSystemThreadPool>(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, std::thread::hardware_concurrency());

    static BPLayerInterfaceImpl broadPhaseLayers;
    static ObjectVsBroadPhaseLayerFilterImpl objectVsBroadphaseFilter;
    static ObjectLayerPairFilterImpl objectPairFilter;

    physicsSystem_ = std::make_unique<PhysicsSystem>();
    physicsSystem_->Init(MAX_BODIES,
                         NUM_BODY_MUTEXES,
                         MAX_BODY_PAIRS,
                         MAX_CONTACT_CONSTRAINTS,
                         broadPhaseLayers,
                         objectVsBroadphaseFilter,
                         objectPairFilter);

    physicsSystem_->SetGravity(Vec3(0, -9.8f, 0));
}

PhysicsWorld::~PhysicsWorld() {
    playerController_.reset();
    physicsSystem_.reset();
    jobSystem_.reset();
    tempAllocator_.reset();
}

void PhysicsWorld::update(float deltaTime) {
    if (!physicsSystem_) return;

    physicsSystem_->Update(deltaTime, 1, tempAllocator_.get(), jobSystem_.get());
    if (playerController_) {
        playerController_->update(deltaTime);
    }
}

void PhysicsWorld::setGravity(float gravity) {
    if (physicsSystem_) physicsSystem_->SetGravity(Vec3(0, gravity, 0));
}

PhysicsRigidBody PhysicsWorld::createBoxBody(const glm::vec3& halfExtents,
                                             float mass,
                                             const glm::vec3& position,
                                             const ::PhysicsMaterial& material) {
    if (!physicsSystem_) return PhysicsRigidBody();

    RefConst<Shape> shape = new BoxShape(toJph(halfExtents));

    const bool dynamic = mass > 0.0f;
    BodyCreationSettings settings(shape,
                                  RVec3(position.x, position.y, position.z),
                                  Quat::sIdentity(),
                                  dynamic ? EMotionType::Dynamic : EMotionType::Static,
                                  dynamic ? Moving : NonMoving);

    if (dynamic) {
        settings.mOverrideMassProperties = EOverrideMassProperties::CalculateInertia;
        settings.mMassPropertiesOverride.mMass = mass;
    }
    settings.mFriction = material.friction;
    settings.mRestitution = material.restitution;

    BodyInterface& bi = physicsSystem_->GetBodyInterface();
    Body* body = bi.CreateBody(settings);
    if (!body) {
        spdlog::error("Failed to create Jolt body");
        return PhysicsRigidBody();
    }

    bi.AddBody(body->GetID(), dynamic ? EActivation::Activate : EActivation::DontActivate);
    return PhysicsRigidBody(this, body->GetID());
}

PhysicsPlayerController& PhysicsWorld::createPlayer(const glm::vec3& size) {
    const glm::vec3 halfExtents = size * 0.5f;
    PhysicsPlayerController controller(this, halfExtents, glm::vec3(0.0f, 2.0f, 0.0f));
    controller.setRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
    controller.setVelocity(glm::vec3(0.0f));
    controller.setAngularVelocity(glm::vec3(0.0f));
    playerController_ = std::make_unique<PhysicsPlayerController>(std::move(controller));
    spdlog::info("Created kinematic player controller");
    return *playerController_;
}

PhysicsPlayerController& PhysicsWorld::createPlayer() {
    return createPlayer(glm::vec3(1.0f, 2.0f, 1.0f));
}

PhysicsStaticBody PhysicsWorld::createStaticMesh(const std::string& meshPath) {
    return PhysicsStaticBody::fromMesh(this, meshPath);
}

bool PhysicsWorld::raycast(const glm::vec3& from, const glm::vec3& to, glm::vec3& hitPoint, glm::vec3& hitNormal) const {
    if (!physicsSystem_) return false;

    RVec3 origin(from.x, from.y, from.z);
    Vec3 direction(to.x - from.x, to.y - from.y, to.z - from.z);
    RRayCast ray(origin, direction);
    RayCastResult result;
    if (!physicsSystem_->GetNarrowPhaseQuery().CastRay(ray, result)) {
        return false;
    }

    glm::vec3 rayVec = glm::vec3(direction.GetX(), direction.GetY(), direction.GetZ());
    hitPoint = glm::vec3(from) + rayVec * result.mFraction;

    BodyLockRead lock(physicsSystem_->GetBodyLockInterface(), result.mBodyID);
    if (lock.Succeeded()) {
        hitNormal = toGlm(lock.GetBody().GetWorldSpaceSurfaceNormal(result.mSubShapeID2, ray.GetPointOnRay(result.mFraction)));
    } else {
        hitNormal = glm::vec3(0.0f);
    }
    return true;
}

void PhysicsWorld::removeBody(const JPH::BodyID& id) const {
    if (!physicsSystem_) return;
    BodyInterface& bi = physicsSystem_->GetBodyInterface();
    if (!bi.IsAdded(id)) return;

    bi.RemoveBody(id);
    bi.DestroyBody(id);
}
