#pragma once

#include "core/types.hpp"
#include "common/json.hpp"

namespace game_world {

PlayerParameters ExtractDefaultPlayerParameters(const karma::json::Value& config);

} // namespace game_world
