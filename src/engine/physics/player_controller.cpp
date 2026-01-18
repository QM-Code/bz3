#include "engine/physics/player_controller.hpp"
#include "engine/physics/physics_world.hpp"
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Geometry/Plane.h>
#include <glm/gtc/quaternion.hpp>
#include <cfloat>
#include <algorithm>

namespace {
using namespace JPH;

inline Vec3 toJph(const glm::vec3& v) { return Vec3(v.x, v.y, v.z); }
inline Quat toJph(const glm::quat& q) { return Quat(q.x, q.y, q.z, q.w); }
inline glm::vec3 toGlm(const Vec3& v) { return glm::vec3(v.GetX(), v.GetY(), v.GetZ()); }
inline glm::vec3 toGlmRVec(const RVec3& v) { return glm::vec3(static_cast<float>(v.GetX()), static_cast<float>(v.GetY()), static_cast<float>(v.GetZ())); }

RefConst<Shape> makeBoxFromHalfExtents(const glm::vec3& halfExtents) {
	return new BoxShape(toJph(halfExtents));
}
}

PhysicsPlayerController::PhysicsPlayerController(PhysicsWorld* world, const glm::vec3& halfExtents, const glm::vec3& startPosition)
	: world(world), halfExtents(halfExtents) {
    if (!world || !world->physicsSystem()) return;

    CharacterVirtualSettings settings;
	settings.mShape = makeBoxFromHalfExtents(halfExtents);
    settings.mMaxSlopeAngle = DegreesToRadians(50.0f);
	settings.mBackFaceMode = EBackFaceMode::IgnoreBackFaces;
	settings.mCharacterPadding = characterPadding;
	settings.mUp = Vec3::sAxisY();

    RVec3 start = RVec3(startPosition.x, startPosition.y + halfExtents.y, startPosition.z);
    character_ = new CharacterVirtual(&settings, start, Quat::sIdentity(), world->physicsSystem());
}

PhysicsPlayerController::PhysicsPlayerController(PhysicsPlayerController&& other) noexcept {
	*this = std::move(other);
}

PhysicsPlayerController& PhysicsPlayerController::operator=(PhysicsPlayerController&& other) noexcept {
	if (this != &other) {
		world = other.world; other.world = nullptr;
        character_ = std::move(other.character_);
        rotation = other.rotation;
        velocity = other.velocity;
        angularVelocity = other.angularVelocity;
        halfExtents = other.halfExtents;
		gravity = other.gravity;
	}
	return *this;
}

PhysicsPlayerController::~PhysicsPlayerController() {
	destroy();
}

void PhysicsPlayerController::setHalfExtents(const glm::vec3& extents) {
	halfExtents = extents;

	if (!character_) return;

	RefConst<Shape> newShape = makeBoxFromHalfExtents(extents);
	JPH::TempAllocator* allocator = world ? world->tempAllocator_.get() : nullptr;
	if (!allocator) return;

	BroadPhaseLayerFilter bpFilter;
	ObjectLayerFilter objFilter;
	BodyFilter bodyFilter;
	ShapeFilter shapeFilter;
	character_->SetShape(newShape.GetPtr(), FLT_MAX, bpFilter, objFilter, bodyFilter, shapeFilter, *allocator);
}

glm::vec3 PhysicsPlayerController::getPosition() const {
	if (!character_) return glm::vec3(0.0f);
	auto pos = character_->GetPosition();
	float offsetY = halfExtents.y + characterPadding; // include padding Jolt adds around the shape
	return glm::vec3(static_cast<float>(pos.GetX()), static_cast<float>(pos.GetY()), static_cast<float>(pos.GetZ())) - glm::vec3(0.0f, offsetY, 0.0f);
}
glm::quat PhysicsPlayerController::getRotation() const { return rotation; }
glm::vec3 PhysicsPlayerController::getVelocity() const { return velocity; }
glm::vec3 PhysicsPlayerController::getAngularVelocity() const { return angularVelocity; }

glm::vec3 PhysicsPlayerController::getForwardVector() const {
	// Assuming -Z forward in local space
	return rotation * glm::vec3(0, 0, -1);
}

void PhysicsPlayerController::setPosition(const glm::vec3& p) {
	if (!character_) return;
	float offsetY = halfExtents.y + characterPadding;
	character_->SetPosition(RVec3(p.x, p.y + offsetY, p.z));
}
void PhysicsPlayerController::setRotation(const glm::quat& r) {
	rotation = glm::normalize(r);
	if (character_) character_->SetRotation(toJph(rotation));
}
void PhysicsPlayerController::setVelocity(const glm::vec3& v) { velocity = v; }
void PhysicsPlayerController::setAngularVelocity(const glm::vec3& w) { angularVelocity = w; }

bool PhysicsPlayerController::isGrounded() const {
    if (!character_) return false;
    using Ground = JPH::CharacterBase::EGroundState;
    Ground state = character_->GetGroundState();
	if (state != Ground::OnGround) return false;

	// Require that the supporting contact lies within a narrow band at the bottom of the shape.
	// This reduces false positives from side contacts (e.g., wedging a corner into a wall).
	RVec3 groundPos = character_->GetGroundPosition();
	RVec3 charPos = character_->GetPosition();
	glm::mat3 invRot = glm::mat3_cast(glm::conjugate(rotation));
	glm::vec3 local = invRot * toGlmRVec(groundPos - charPos);
	float supportCeiling = -halfExtents.y + groundSupportBand;
	return local.y <= supportCeiling;
}

void PhysicsPlayerController::update(float dt) {
	if (!world || !character_ || dt <= 0.f) return;

	Vec3 gravityVec = world->physicsSystem() ? world->physicsSystem()->GetGravity() : Vec3(0, gravity, 0);
	const bool grounded = isGrounded();
	glm::vec3 desiredVelocity = velocity;
	if (!(grounded && velocity.y <= 0.0f)) {
		velocity += toGlm(gravityVec) * dt;
	} else if (velocity.y < 0.0f) {
		velocity.y = 0.0f; // prevent gravity accumulation while supported
	}

	character_->SetRotation(toJph(rotation));
	glm::vec3 preUpdateVelocity = velocity;
	character_->SetLinearVelocity(toJph(velocity));

	CharacterVirtual::ExtendedUpdateSettings updateSettings;
	// Disable stair stepping while airborne to avoid climbing mid-air contacts
	if (!grounded) {
		updateSettings.mWalkStairsStepUp = Vec3::sZero();
	}
	BroadPhaseLayerFilter bpFilter;
	ObjectLayerFilter objFilter;
	BodyFilter bodyFilter;
	ShapeFilter shapeFilter;
	JPH::TempAllocator* allocator = world->tempAllocator_.get();
	if (!allocator) return;
	character_->ExtendedUpdate(dt,
	                         gravityVec,
	                         updateSettings,
	                         bpFilter,
	                         objFilter,
	                         bodyFilter,
	                         shapeFilter,
	                         *allocator);

	velocity = toGlm(character_->GetLinearVelocity());

	// If airborne and lateral velocity was significantly redirected or halted by a collision, stop angular turn.
	if (!grounded) {
		glm::vec3 preH = glm::vec3(preUpdateVelocity.x, 0.0f, preUpdateVelocity.z);
		glm::vec3 postH = glm::vec3(velocity.x, 0.0f, velocity.z);
		float preLen = glm::length(preH);
		float postLen = glm::length(postH);
		if (preLen > 1e-4f && postLen > 1e-4f) {
			float align = glm::dot(preH / preLen, postH / postLen);
			if (align < 0.8f || postLen < preLen * 0.5f) {
				angularVelocity = glm::vec3(0.0f);
			}
		} else if (preLen > 1e-3f && postLen < 1e-3f) {
			angularVelocity = glm::vec3(0.0f);
		}
	}

	if (glm::dot(angularVelocity, angularVelocity) > 0.f) {
		glm::quat dq = glm::quat(0, angularVelocity.x, angularVelocity.y, angularVelocity.z) * rotation;
		rotation = glm::normalize(rotation + 0.5f * dq * dt);
	}
}

void PhysicsPlayerController::destroy() {
	character_ = nullptr;
	world = nullptr;
}

glm::vec3 PhysicsPlayerController::centerPosition() const {
	return getPosition() + glm::vec3(0.0f, halfExtents.y + characterPadding, 0.0f);
}
