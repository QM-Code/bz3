#pragma once

#include <string>
#include <vector>

#include "core/types.hpp"
#include "platform/events.hpp"

namespace platform {
class Window;
}

class Input {
    friend class ClientEngine;

public:
    struct Binding {
        enum class Type { Key, MouseButton } type;
        platform::Key key = platform::Key::Unknown;
        platform::MouseButton mouseButton = platform::MouseButton::Left;
    };

private:
    struct KeyBindings {
        std::vector<Binding> fire;
        std::vector<Binding> spawn;
        std::vector<Binding> jump;
        std::vector<Binding> quickQuit;
        std::vector<Binding> chat;
        std::vector<Binding> escape;
        std::vector<Binding> toggleFullscreen;
        std::vector<Binding> moveLeft;
        std::vector<Binding> moveRight;
        std::vector<Binding> moveForward;
        std::vector<Binding> moveBackward;
    } keyBindings;

    InputState inputState;
    platform::Window *window = nullptr;

    Input(platform::Window &window);
    ~Input() = default;

    void loadKeyBindings();
    bool keyMatches(const std::vector<Binding> &binding, platform::Key key) const;
    bool mouseMatches(const std::vector<Binding> &binding, platform::MouseButton button) const;
    bool isBindingPressed(const std::vector<Binding> &binding) const;
    void update(const std::vector<platform::Event> &events);

public:
    const InputState &getInputState() const;
    void clearState();
    std::string spawnHintText() const;
    std::string bindingListDisplay(const std::vector<Binding> &bindings) const;
};
