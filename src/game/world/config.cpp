#include "game/world/config.hpp"

namespace game_world {

PlayerParameters ExtractDefaultPlayerParameters(const bz::json::Value& config) {
    PlayerParameters params;
    if (!config.is_object()) {
        return params;
    }
    if (!config.contains("defaultPlayerParameters")) {
        return params;
    }
    const auto& defaults = config["defaultPlayerParameters"];
    if (!defaults.is_object()) {
        return params;
    }
    for (const auto& [key, value] : defaults.items()) {
        if (value.is_number()) {
            params[key] = value.get<float>();
        }
    }
    return params;
}

} // namespace game_world
