#pragma once

#include "karma/ecs/registry.hpp"
#include "karma/ecs/types.hpp"
#include <vector>
#include <vector>

namespace ecs {

class World {
public:
    World() = default;

    EntityId createEntity() {
        return ++nextId_;
    }

    void destroyEntity(EntityId entity) {
        if (entity == kInvalidEntity) {
            return;
        }
        destroyed_.push_back(entity);
    }

    std::vector<EntityId> consumeDestroyed() {
        std::vector<EntityId> out = std::move(destroyed_);
        destroyed_.clear();
        return out;
    }

    template <typename T>
    void set(EntityId entity, const T &component) {
        storeMutable<T>().set(entity, component);
    }

    template <typename T>
    void remove(EntityId entity) {
        storeMutable<T>().remove(entity);
    }

    template <typename T>
    T *get(EntityId entity) {
        return storeMutable<T>().get(entity);
    }

    template <typename T>
    const T *get(EntityId entity) const {
        return storeConst<T>().get(entity);
    }

    template <typename T>
    const std::unordered_map<EntityId, T> &all() const {
        return storeConst<T>().all();
    }

    void clear() {
        nextId_ = kInvalidEntity;
    }

private:
    EntityId nextId_ = kInvalidEntity;
    std::vector<EntityId> destroyed_{};

    template <typename T>
    static ComponentStore<T> &store() {
        static ComponentStore<T> store;
        return store;
    }

    template <typename T>
    ComponentStore<T> &storeMutable() {
        return store<T>();
    }

    template <typename T>
    const ComponentStore<T> &storeConst() const {
        return store<T>();
    }
};

} // namespace ecs
