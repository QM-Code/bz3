#include "engine/physics/player_controller.hpp"
#include "engine/physics/physics_world.hpp"
#include <btBulletDynamicsCommon.h>
#include <BulletCollision/NarrowPhaseCollision/btGjkEpaPenetrationDepthSolver.h>
#include <BulletCollision/NarrowPhaseCollision/btGjkPairDetector.h>
#include <BulletCollision/NarrowPhaseCollision/btPointCollector.h>
#include <BulletCollision/NarrowPhaseCollision/btVoronoiSimplexSolver.h>

namespace {
// Convert stored glm quaternion to Bullet's quaternion (x, y, z, w order).
btQuaternion toBtQuat(const glm::quat& q) { return btQuaternion(q.x, q.y, q.z, q.w); }

btTransform makeTransform(const glm::vec3& p, const glm::quat& r) {
	btTransform t; t.setIdentity();
	t.setOrigin(btVector3(p.x, p.y, p.z));
	t.setRotation(toBtQuat(r));
	return t;
}
}

PhysicsPlayerController::PhysicsPlayerController(PhysicsWorld* world, const glm::vec3& halfExtents, const glm::vec3& startPosition)
	: world(world), position(startPosition), halfExtents(halfExtents) {}

PhysicsPlayerController::PhysicsPlayerController(PhysicsPlayerController&& other) noexcept {
	*this = std::move(other);
}

PhysicsPlayerController& PhysicsPlayerController::operator=(PhysicsPlayerController&& other) noexcept {
	if (this != &other) {
		world = other.world; other.world = nullptr;
		position = other.position;
		rotation = other.rotation;
		velocity = other.velocity;
		angularVelocity = other.angularVelocity;
		halfExtents = other.halfExtents;
	}
	return *this;
}

PhysicsPlayerController::~PhysicsPlayerController() {
	destroy();
}

glm::vec3 PhysicsPlayerController::getPosition() const {
	return position - glm::vec3(0.0f, halfExtents.y, 0.0f);
}
glm::quat PhysicsPlayerController::getRotation() const { return rotation; }
glm::vec3 PhysicsPlayerController::getVelocity() const { return velocity; }
glm::vec3 PhysicsPlayerController::getAngularVelocity() const { return angularVelocity; }

glm::vec3 PhysicsPlayerController::getForwardVector() const {
	// Assuming -Z forward in local space
	return rotation * glm::vec3(0, 0, -1);
}

void PhysicsPlayerController::setPosition(const glm::vec3& p) {
	position = p + glm::vec3(0.0f, halfExtents.y, 0.0f);
}
void PhysicsPlayerController::setRotation(const glm::quat& r) { rotation = glm::normalize(r); }
void PhysicsPlayerController::setVelocity(const glm::vec3& v) { velocity = v; }
void PhysicsPlayerController::setAngularVelocity(const glm::vec3& w) { angularVelocity = w; }

bool PhysicsPlayerController::isGrounded() const {
	if (!world || !world->world_) return false;

	// Slightly shrink lateral extents to avoid sticking when brushing walls mid-jump.
	const glm::vec3 groundedHalfExtents = glm::max(halfExtents - glm::vec3(0.02f, 0.0f, 0.02f), glm::vec3(0.01f));
	btVector3 half(groundedHalfExtents.x, groundedHalfExtents.y, groundedHalfExtents.z);
	btBoxShape boxShape(half);
	boxShape.setMargin(0.0f);

	btTransform from = makeTransform(position, rotation);
	btTransform to   = makeTransform(glm::vec3(position.x, position.y - 0.05f, position.z), rotation);

	btCollisionWorld::ClosestConvexResultCallback cb(from.getOrigin(), to.getOrigin());
	cb.m_collisionFilterGroup = btBroadphaseProxy::DefaultFilter;
	cb.m_collisionFilterMask = btBroadphaseProxy::AllFilter;

	world->world_->convexSweepTest(&boxShape, from, to, cb);
	return cb.hasHit();
}

void PhysicsPlayerController::update(float dt) {
	if (!world) return;
	if (dt <= 0.f) return;

	// Apply gravity
	velocity.y += gravity * dt;

	btVector3 half(halfExtents.x, halfExtents.y, halfExtents.z);
	btBoxShape boxShape(half);
	boxShape.setMargin(0.0f);

	float remaining = dt;
	const int maxIters = 3;
	for (int iter = 0; iter < maxIters && remaining > 0.f; ++iter) {
		const glm::vec3 start = position;
		const glm::vec3 target = start + velocity * remaining;

		btTransform from = makeTransform(start, rotation);
		btTransform to   = makeTransform(target, rotation);

		btCollisionWorld::ClosestConvexResultCallback cb(from.getOrigin(), to.getOrigin());
		cb.m_collisionFilterGroup = btBroadphaseProxy::DefaultFilter;
		cb.m_collisionFilterMask = btBroadphaseProxy::AllFilter;

		world->world_->convexSweepTest(&boxShape, from, to, cb);

		float fraction = cb.hasHit() ? cb.m_closestHitFraction : 1.0f;
		fraction = glm::clamp(fraction, 0.0f, 1.0f);
		position = start + (target - start) * fraction;

		if (!cb.hasHit()) break;

		btVector3 nbt = cb.m_hitNormalWorld;
		if (nbt.length2() <= btScalar(0)) break;
		nbt.normalize();
		glm::vec3 n(nbt.x(), nbt.y(), nbt.z());

		const float vn = glm::dot(velocity, n);
		velocity -= vn * n; // slide by removing normal component

		remaining *= (1.0f - fraction);
		if (remaining <= 0.f) break;
		if (glm::dot(velocity, velocity) < 1e-6f) break;
	}

	// Apply angular velocity to rotation (simple integrator).
	if (glm::dot(angularVelocity, angularVelocity) > 0.f) {
		glm::quat dq = glm::quat(0, angularVelocity.x, angularVelocity.y, angularVelocity.z) * rotation;
		rotation = glm::normalize(rotation + 0.5f * dq * dt);
	}

	resolvePenetrations();
}

void PhysicsPlayerController::resolvePenetrations() {
	if (!world || !world->world_) return;

	const int maxIterations = 6;
	for (int i = 0; i < maxIterations; ++i) {
		glm::vec3 normal;
		float depth = 0.0f;
		if (!findDeepestPenetration(normal, depth)) break;
		if (depth >= 0.0f) break;

		const float push = -depth;
		position += normal * push;

		const float vn = glm::dot(velocity, normal);
		if (vn < 0.0f) {
			velocity -= vn * normal; // keep moving instead of re-embedding
		}
	}
}

bool PhysicsPlayerController::findDeepestPenetration(glm::vec3& outNormal, float& outDepth) const {
	if (!world || !world->world_) return false;

	btVector3 half(halfExtents.x, halfExtents.y, halfExtents.z);
	btBoxShape playerShape(half);
	playerShape.setMargin(0.0f);

	btTransform playerTransform = makeTransform(position, rotation);

	struct DeepestContactCallback : btCollisionWorld::ContactResultCallback {
		float deepest = 0.0f; // most negative
		btVector3 normal{0,0,0};
		bool found = false;
		btScalar addSingleResult(btManifoldPoint& cp,
			const btCollisionObjectWrapper* colObj0Wrap,int /*partId0*/,int /*index0*/,const btCollisionObjectWrapper* /*colObj1Wrap*/,int /*partId1*/,int /*index1*/) override {
			if (cp.m_distance1 < deepest) {
				deepest = cp.m_distance1;
				normal = cp.m_normalWorldOnB; // points from B to A
				found = true;
			}
			return 0.0f;
		}
	};

	btCollisionObject playerObj;
	playerObj.setCollisionShape(&playerShape);
	playerObj.setWorldTransform(playerTransform);
	playerObj.setCollisionFlags(playerObj.getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);

	DeepestContactCallback cb;
	cb.m_collisionFilterGroup = btBroadphaseProxy::DefaultFilter;
	cb.m_collisionFilterMask = btBroadphaseProxy::AllFilter;

	const int count = world->world_->getNumCollisionObjects();
	for (int i = 0; i < count; ++i) {
		btCollisionObject* obj = world->world_->getCollisionObjectArray()[i];
		if (!obj) continue;

		world->world_->contactPairTest(&playerObj, obj, cb);
	}

	if (!cb.found || cb.deepest >= 0.0f) return false;

	btVector3 n = cb.normal;
	if (n.length2() <= btScalar(0)) return false;
	n.normalize();
	outNormal = glm::vec3(n.x(), n.y(), n.z());
	outDepth = cb.deepest;
	return true;
}

void PhysicsPlayerController::destroy() {
	world = nullptr;
}
