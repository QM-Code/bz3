#pragma once
#include <functional>
#include <GLFW/glfw3.h>

struct GLFWUserPointer {
    std::function<void(int, int)> resizeCallback;
    std::function<void(GLFWwindow*, int, int, int, int)> keyCallback;
};