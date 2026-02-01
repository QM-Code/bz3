#pragma once

#include "karma/ecs/components.hpp"
#include "karma/ecs/world.hpp"
#include "karma/physics/physics_world.hpp"
#include "karma/physics/rigid_body.hpp"
#include "karma/physics/static_body.hpp"
#include <unordered_map>

namespace ecs {

class PhysicsSyncSystem {
public:
    void update(World &world, PhysicsWorld *physics) {
        if (!physics) {
            return;
        }

        // Cleanup stale entries.
        for (auto it = rigidBodies_.begin(); it != rigidBodies_.end(); ) {
            const EntityId entity = it->first;
            if (!hasComponent<ColliderComponent>(world, entity) ||
                !hasComponent<RigidbodyComponent>(world, entity) ||
                !hasComponent<Transform>(world, entity)) {
                it->second.destroy();
                it = rigidBodies_.erase(it);
            } else {
                ++it;
            }
        }
        for (auto it = staticBodies_.begin(); it != staticBodies_.end(); ) {
            const EntityId entity = it->first;
            if (!hasComponent<ColliderComponent>(world, entity) ||
                !hasComponent<Transform>(world, entity)) {
                it->second.destroy();
                it = staticBodies_.erase(it);
            } else {
                ++it;
            }
        }

        const auto &transforms = world.all<Transform>();
        const auto &colliders = world.all<ColliderComponent>();
        const auto &rigidbodies = world.all<RigidbodyComponent>();

        for (const auto &pair : colliders) {
            const EntityId entity = pair.first;
            const ColliderComponent &collider = pair.second;
            const auto transformIt = transforms.find(entity);
            if (transformIt == transforms.end()) {
                continue;
            }
            const Transform &transform = transformIt->second;

            const bool hasRigidBody = rigidbodies.find(entity) != rigidbodies.end();
            if (hasRigidBody) {
                if (rigidBodies_.find(entity) == rigidBodies_.end()) {
                    if (collider.shape == ColliderComponent::Shape::Box) {
                        const RigidbodyComponent &rb = rigidbodies.at(entity);
                        PhysicsRigidBody body = physics->createBoxBody(
                            collider.half_extents,
                            rb.mass,
                            transform.position,
                            collider.material);
                        rigidBodies_.insert({entity, std::move(body)});
                    }
                }
            } else if (collider.shape == ColliderComponent::Shape::Mesh) {
                if (staticBodies_.find(entity) == staticBodies_.end() && !collider.mesh_key.empty()) {
                    PhysicsStaticBody body = physics->createStaticMesh(collider.mesh_key);
                    staticBodies_.insert({entity, std::move(body)});
                }
            }
        }

        for (auto &pair : rigidBodies_) {
            const EntityId entity = pair.first;
            if (auto *transform = world.get<Transform>(entity)) {
                const auto rbIt = rigidbodies.find(entity);
                if (rbIt != rigidbodies.end()) {
                    const RigidbodyComponent &rb = rbIt->second;
                    if (rb.kinematic) {
                        pair.second.setPosition(transform->position);
                        pair.second.setRotation(transform->rotation);
                    } else {
                        transform->position = pair.second.getPosition();
                        transform->rotation = pair.second.getRotation();
                    }
                }
            }
        }
    }

private:
    std::unordered_map<EntityId, PhysicsRigidBody> rigidBodies_;
    std::unordered_map<EntityId, PhysicsStaticBody> staticBodies_;

    template <typename T>
    bool hasComponent(const World &world, EntityId entity) const {
        const auto &store = world.all<T>();
        return store.find(entity) != store.end();
    }
};

} // namespace ecs
