#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <span>

namespace ui::bindings {

struct BindingDefinition {
    const char *action;
    const char *label;
};

std::span<const BindingDefinition> Definitions();
bool IsMouseBindingName(std::string_view name);
std::string JoinBindings(const std::vector<std::string> &entries);
std::vector<std::string> SplitBindings(const std::string &text);

} // namespace ui::bindings
