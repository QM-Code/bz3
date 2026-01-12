#pragma once
#include <GLFW/glfw3.h>
#include <glm/vec2.hpp>
#include "engine/types.hpp"

class Input {
    friend class ClientEngine;

private:
    InputState inputState;
    GLFWwindow *window;

    Input(GLFWwindow *window);
    ~Input() = default;

    void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
    void update();

public:
    const InputState &getInputState() const;
};