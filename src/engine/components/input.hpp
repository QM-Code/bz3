#pragma once
#include <GLFW/glfw3.h>
#include <glm/vec2.hpp>
#include <vector>
#include <string>
#include "engine/types.hpp"

class Input {
    friend class ClientEngine;

public:
    struct Binding {
        enum class Type { Key, MouseButton } type;
        int code;
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
    GLFWwindow *window;

    Input(GLFWwindow *window);
    ~Input() = default;

    void loadKeyBindings();
    bool keyMatches(const std::vector<Binding> &binding, int key) const;
    bool mouseMatches(const std::vector<Binding> &binding, int button) const;
    bool isBindingPressed(const std::vector<Binding> &binding) const;
    void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
    void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
    void update();

public:
    const InputState &getInputState() const;
    void clearState();
    std::string spawnHintText() const;
    std::string bindingListDisplay(const std::vector<Binding> &bindings) const;
};
