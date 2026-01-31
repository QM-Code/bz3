#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace input::bindings {

bool IsMouseBindingName(std::string_view name);
std::string JoinBindings(const std::vector<std::string> &entries);
std::vector<std::string> SplitBindings(const std::string &text);

} // namespace input::bindings
