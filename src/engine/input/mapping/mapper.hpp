#pragma once

#include "input/mapping/map.hpp"

#include "platform/events.hpp"

#include <vector>

namespace platform {
class Window;
}

namespace input {

class InputMapper {
public:
    void loadBindings(const bz::json::Value* keybindings, const InputMap::DefaultBindingsMap& defaults);

    bool actionTriggered(std::string_view actionId, const std::vector<platform::Event>& events) const;
    bool actionDown(std::string_view actionId, const platform::Window* window) const;

    const std::vector<Binding>& bindings(std::string_view actionId) const { return map_.bindings(actionId); }
    std::string bindingListDisplay(std::string_view actionId) const { return map_.bindingListDisplay(actionId); }

private:
    InputMap map_{};
};

} // namespace input
