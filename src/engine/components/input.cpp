#include "engine/components/input.hpp"
#include "engine/types.hpp"
#include "engine/user_pointer.hpp"

Input::Input(GLFWwindow *window) {
    this->window = window;
    this->inputState = {};

    auto* userPointer = static_cast<GLFWUserPointer*>(glfwGetWindowUserPointer(window));
    userPointer->keyCallback = [this](GLFWwindow* window, int key, int scancode, int action, int mods) {
        this->keyCallback(window, key, scancode, action, mods);
    };

    glfwSetKeyCallback(window, [](GLFWwindow* w, int key, int scancode, int action, int mods) {
        auto* userPointer = static_cast<GLFWUserPointer*>(glfwGetWindowUserPointer(w));
        userPointer->keyCallback(w, key, scancode, action, mods);
    });
}

void Input::keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (action == GLFW_PRESS) {
        // Fire
        if (key == GLFW_KEY_F || key == GLFW_KEY_E) {
            inputState.fire = true;
        }

        // Spawn
        if (key == GLFW_KEY_U) {
            inputState.spawn = true;
        }

        if (key == GLFW_KEY_F12) {
            inputState.quickQuit = true;
        }

        if (key == GLFW_KEY_RIGHT_BRACKET) {
            inputState.toggleFullscreen = true;
        }

        if (key == GLFW_KEY_T) {
            // Start chat
            inputState.chat = true;
        }

        if (key == GLFW_KEY_ESCAPE) {
            inputState.escape = true;
        }
    }
}

void Input::update() {
    inputState = { 0 };

    if (glfwGetKey(window, GLFW_KEY_LEFT) || glfwGetKey(window, GLFW_KEY_J))
        inputState.movement.x -= 1;
    if (glfwGetKey(window, GLFW_KEY_RIGHT) || glfwGetKey(window, GLFW_KEY_L))
        inputState.movement.x += 1;

    // Movement
    if (glfwGetKey(window, GLFW_KEY_UP) || glfwGetKey(window, GLFW_KEY_I))
        inputState.movement.y += 1;
    if (glfwGetKey(window, GLFW_KEY_DOWN) || glfwGetKey(window, GLFW_KEY_K))
        inputState.movement.y -= 1;

    // Jump
    if (glfwGetKey(window, GLFW_KEY_SPACE)) {
        inputState.jump = true;
    }

    glfwPollEvents();
}

const InputState &Input::getInputState() const {
    return inputState;
}