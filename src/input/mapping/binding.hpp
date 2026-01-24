#pragma once

#include "platform/events.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace input {

struct Binding {
    enum class Type { Key, MouseButton } type;
    platform::Key key = platform::Key::Unknown;
    platform::MouseButton mouseButton = platform::MouseButton::Left;
};

std::optional<Binding> BindingFromName(std::string_view name);
std::string BindingToString(const Binding& binding);
std::string JoinBindingStrings(const std::vector<Binding>& bindings);

} // namespace input
