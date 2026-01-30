#pragma once

#include "engine/ecs/types.hpp"
#include <optional>
#include <unordered_map>

namespace ecs {

template <typename T>
class ComponentStore {
public:
    void set(EntityId entity, const T &component) { components_[entity] = component; }
    void remove(EntityId entity) { components_.erase(entity); }
    bool has(EntityId entity) const { return components_.find(entity) != components_.end(); }
    T *get(EntityId entity) {
        auto it = components_.find(entity);
        return it == components_.end() ? nullptr : &it->second;
    }
    const std::unordered_map<EntityId, T> &all() const { return components_; }

private:
    std::unordered_map<EntityId, T> components_;
};

} // namespace ecs
