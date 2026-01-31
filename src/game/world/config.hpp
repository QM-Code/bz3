#pragma once

#include "karma/core/types.hpp"
#include "karma/common/json.hpp"

namespace game_world {

PlayerParameters ExtractDefaultPlayerParameters(const karma::json::Value& config);

} // namespace game_world
