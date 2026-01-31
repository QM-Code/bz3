#pragma once

#include "common/json.hpp"
#include "input/mapping/binding.hpp"

#include <string_view>
#include <string>
#include <unordered_map>
#include <vector>

namespace input {

class InputMap {
public:
    using DefaultBindingsMap = std::unordered_map<std::string, std::vector<std::string>>;

    void load(const karma::json::Value* keybindings, const DefaultBindingsMap& defaults);
    const std::vector<Binding>& bindings(std::string_view actionId) const;
    std::string bindingListDisplay(std::string_view actionId) const;

private:
    std::unordered_map<std::string, std::vector<Binding>> bindings_{};
};

} // namespace input
