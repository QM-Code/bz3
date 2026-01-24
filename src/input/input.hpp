#pragma once

#include <string>
#include <vector>

#include "core/types.hpp"
#include "platform/events.hpp"
#include "input/mapping/mapper.hpp"

namespace platform {
class Window;
}

class Input {
    friend class ClientEngine;

private:
    input::InputMapper mapper_;

    InputState inputState;
    platform::Window *window = nullptr;

    Input(platform::Window &window);
    ~Input() = default;

    void loadKeyBindings();
    void update(const std::vector<platform::Event> &events);

public:
    const InputState &getInputState() const;
    void clearState();
    void reloadKeyBindings();
    std::string spawnHintText() const;
    std::string bindingListDisplay(input::Action action) const;
};
