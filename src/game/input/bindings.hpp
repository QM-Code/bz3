#pragma once

#include "game/input/actions.hpp"

#include <string>
#include <unordered_map>
#include <vector>

namespace game_input {

using DefaultBindingsMap = std::unordered_map<std::string, std::vector<std::string>>;

DefaultBindingsMap DefaultKeybindings();

} // namespace game_input
