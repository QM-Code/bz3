#include "engine/physics/player_controller.hpp"
#include "engine/physics/physics_world.hpp"
#include <btBulletDynamicsCommon.h>

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
		skin = other.skin;
	}
	return *this;
}

PhysicsPlayerController::~PhysicsPlayerController() {
	destroy();
}

glm::vec3 PhysicsPlayerController::getPosition() const { return position; }
glm::quat PhysicsPlayerController::getRotation() const { return rotation; }
glm::vec3 PhysicsPlayerController::getVelocity() const { return velocity; }
glm::vec3 PhysicsPlayerController::getAngularVelocity() const { return angularVelocity; }

glm::vec3 PhysicsPlayerController::getForwardVector() const {
	// Assuming -Z forward in local space
	return rotation * glm::vec3(0, 0, -1);
}

void PhysicsPlayerController::setPosition(const glm::vec3& p) { position = p; }
void PhysicsPlayerController::setRotation(const glm::quat& r) { rotation = glm::normalize(r); }
void PhysicsPlayerController::setVelocity(const glm::vec3& v) { velocity = v; }
void PhysicsPlayerController::setAngularVelocity(const glm::vec3& w) { angularVelocity = w; }

bool PhysicsPlayerController::isGrounded() const {
	if (!world || !world->world_) return false;
	// Allow a small upward tolerance so slight bounces don't report airborne.
	if (velocity.y > 0.5f) return false;

	btVector3 half(halfExtents.x, halfExtents.y, halfExtents.z);
	btBoxShape boxShape(half);
	boxShape.setMargin(0.0f);

	btTransform from = makeTransform(position, rotation);
	btTransform to   = from;
	to.setOrigin(from.getOrigin() - btVector3(0, 0.05f, 0));

	btCollisionWorld::ClosestConvexResultCallback cb(from.getOrigin(), to.getOrigin());
	cb.m_collisionFilterGroup = btBroadphaseProxy::DefaultFilter;
	cb.m_collisionFilterMask = btBroadphaseProxy::AllFilter;

	world->world_->convexSweepTest(&boxShape, from, to, cb);

	return cb.hasHit() && cb.m_hitNormalWorld.dot(btVector3(0, 1, 0)) > 0.7f;
}

bool PhysicsPlayerController::sweepTo(const glm::vec3& target, glm::vec3& outHitPoint, glm::vec3& outHitNormal) const {
	if (!world || !world->world_) return false;

	btVector3 half(halfExtents.x, halfExtents.y, halfExtents.z);
	btBoxShape boxShape(half);
	boxShape.setMargin(0.0f);

	btTransform from = makeTransform(position, rotation);
	btTransform to   = makeTransform(target, rotation);

	btCollisionWorld::ClosestConvexResultCallback cb(from.getOrigin(), to.getOrigin());
	cb.m_collisionFilterGroup = btBroadphaseProxy::DefaultFilter;
	cb.m_collisionFilterMask = btBroadphaseProxy::AllFilter;

	world->world_->convexSweepTest(&boxShape, from, to, cb);

	if (cb.hasHit()) {
		outHitPoint = glm::vec3(cb.m_hitPointWorld.x(), cb.m_hitPointWorld.y(), cb.m_hitPointWorld.z());
		btVector3 n = cb.m_hitNormalWorld;
		btScalar len = n.length();
		if (len > btScalar(0)) n /= len;
		outHitNormal = glm::vec3(n.x(), n.y(), n.z());
		return true;
	}
	return false;
}

void PhysicsPlayerController::resolveGround() {
	if (!world) return;
	// Only resolve toward the ground when moving downward or resting; while ascending (jumping)
	// we skip this to avoid snapping back to the floor immediately, but allow a small tolerance to
	// still catch gentle landings.
	if (velocity.y > 0.5f) return;
	// Sweep slightly downward to find support
	btVector3 half(halfExtents.x, halfExtents.y, halfExtents.z);
	btBoxShape boxShape(half);
	boxShape.setMargin(0.0f);

	btTransform from = makeTransform(position, rotation);
	btTransform to   = makeTransform(glm::vec3(position.x, position.y - 0.05f, position.z), rotation);

	btCollisionWorld::ClosestConvexResultCallback cb(from.getOrigin(), to.getOrigin());
	cb.m_collisionFilterGroup = btBroadphaseProxy::DefaultFilter;
	cb.m_collisionFilterMask = btBroadphaseProxy::AllFilter;
	world->world_->convexSweepTest(&boxShape, from, to, cb);

	if (cb.hasHit() && cb.m_hitNormalWorld.dot(btVector3(0,1,0)) > 0.1f) {
		position.y = cb.m_hitPointWorld.y() + halfExtents.y + skin;
		if (velocity.y < 0.0f) velocity.y = 0.0f;
	}
}

void PhysicsPlayerController::update(float dt) {
	if (!world) return;
	if (dt <= 0.f) return;

	// Apply gravity
	velocity.y += gravity * dt;

	float remaining = dt;
	const int maxIter = 3;
	for (int i = 0; i < maxIter && remaining > 0.f; ++i) {
		glm::vec3 target = position + velocity * remaining;

		// Full sweep to get hit fraction and normal
		btVector3 half(halfExtents.x, halfExtents.y, halfExtents.z);
		btBoxShape boxShape(half);
		boxShape.setMargin(0.0f);

		btTransform from = makeTransform(position, rotation);
		btTransform to   = makeTransform(target, rotation);

		btCollisionWorld::ClosestConvexResultCallback cb(from.getOrigin(), to.getOrigin());
		cb.m_collisionFilterGroup = btBroadphaseProxy::DefaultFilter;
		cb.m_collisionFilterMask = btBroadphaseProxy::AllFilter;

		world->world_->convexSweepTest(&boxShape, from, to, cb);

		float frac = cb.hasHit() ? cb.m_closestHitFraction : 1.0f;
		frac = glm::clamp(frac, 0.0f, 1.0f);
		position = position + velocity * (remaining * frac);

		bool validHit = cb.hasHit() && frac < 0.999f;
		btVector3 nbt = cb.m_hitNormalWorld;
		btScalar len = nbt.length();
		if (len > btScalar(0)) nbt /= len; else validHit = false;
		glm::vec3 n(nbt.x(), nbt.y(), nbt.z());
		const float vn = glm::dot(velocity, n);
		if (validHit && vn > 0.0f) validHit = false; // moving away

		if (validHit) {
			// Scale the separation by how directly we’re hitting the surface to reduce jitter on shallow glances
			const float speed = glm::length(velocity);
			const float pushScale = (speed > 1e-4f) ? glm::clamp(std::abs(vn) / speed, 0.0f, 1.0f) : 0.0f;
			const float push = skin * pushScale;
			position += n * push;

			if (vn < 0.0f) {
				velocity -= vn * n; // slide: drop normal component
			}
			remaining *= (1.0f - frac);
		} else {
			remaining = 0.f;
		}
	}

	resolveGround();
	if (isGrounded() && velocity.y < 0.0f) {
		velocity.y = 0.0f;
	}

	// Apply angular velocity to rotation (simple integrator) and resolve any wall contact caused by rotation alone.
	const glm::quat prevRotation = rotation;
	if (glm::dot(angularVelocity, angularVelocity) > 0.f) {
		glm::quat dq = glm::quat(0, angularVelocity.x, angularVelocity.y, angularVelocity.z) * rotation;
		rotation = glm::normalize(rotation + 0.5f * dq * dt);
	}

	// Rotational sweep to avoid embedding when turning in place against geometry.
	if (world && glm::dot(angularVelocity, angularVelocity) > 0.f) {
		btVector3 half(halfExtents.x, halfExtents.y, halfExtents.z);
		btBoxShape boxShape(half);
		boxShape.setMargin(0.0f);

		btTransform from = makeTransform(position, prevRotation);
		btTransform to   = makeTransform(position, rotation);

		btCollisionWorld::ClosestConvexResultCallback cb(from.getOrigin(), to.getOrigin());
		cb.m_collisionFilterGroup = btBroadphaseProxy::DefaultFilter;
		cb.m_collisionFilterMask = btBroadphaseProxy::AllFilter;

		world->world_->convexSweepTest(&boxShape, from, to, cb);

		if (cb.hasHit()) {
			btVector3 nbt = cb.m_hitNormalWorld;
			btScalar len = nbt.length();
			if (len > btScalar(0)) {
				nbt /= len;
				glm::vec3 n(nbt.x(), nbt.y(), nbt.z());
				position += n * (skin + 0.002f);
				float vn = glm::dot(velocity, n);
				if (vn < 0.0f) {
					velocity -= vn * n;
				}
			}
		}
	}
}

void PhysicsPlayerController::destroy() {
	world = nullptr;
}
