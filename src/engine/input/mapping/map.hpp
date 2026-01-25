#pragma once

#include "common/json.hpp"
#include "input/mapping/actions.hpp"
#include "input/mapping/binding.hpp"

#include <array>
#include <string>
#include <vector>

namespace input {

class InputMap {
public:
    void load(const bz::json::Value* keybindings);
    const std::vector<Binding>& bindings(Action action) const;
    std::string bindingListDisplay(Action action) const;

private:
    std::array<std::vector<Binding>, kAllActions.size()> bindings_{};
};

} // namespace input
