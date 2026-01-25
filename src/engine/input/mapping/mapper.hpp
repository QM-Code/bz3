#pragma once

#include "input/mapping/actions.hpp"
#include "input/mapping/map.hpp"

#include "platform/events.hpp"

#include <vector>

namespace platform {
class Window;
}

namespace input {

class InputMapper {
public:
    void loadBindings(const bz::json::Value* keybindings);

    bool actionTriggered(Action action, const std::vector<platform::Event>& events) const;
    bool actionDown(Action action, const platform::Window* window) const;

    const std::vector<Binding>& bindings(Action action) const { return map_.bindings(action); }
    std::string bindingListDisplay(Action action) const { return map_.bindingListDisplay(action); }

private:
    InputMap map_{};
};

} // namespace input
