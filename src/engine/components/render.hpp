#pragma once
#include <threepp/threepp.hpp>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <map>
#include <string>
#include <map>
#include "engine/types.hpp"

#define CAMERA_FOV 60.0f
#define SCREEN_WIDTH 800.0f
#define SCREEN_HEIGHT 600.0f

class Render {
    friend class ClientEngine;

private:
    // Scene
    GLFWwindow *window;
    threepp::GLRenderer renderer;
    std::shared_ptr<threepp::Scene> scene;
    std::shared_ptr<threepp::PerspectiveCamera> camera;
    std::map<render_id, std::shared_ptr<threepp::Group>> objects;

    Render(GLFWwindow *window);
    ~Render();

    void update();
    void resizeCallback(int width, int height);

public:
    render_id create(std::string modelPath);
    void destroy(render_id id);
    void setPosition(render_id id, const glm::vec3 &position);
    void setRotation(render_id id, const glm::quat &rotation);
    void setScale(render_id id, const glm::vec3 &scale);
    void setVisible(render_id id, bool visible);
    void setTransparency(render_id id, bool transparency);
    void setCameraPosition(const glm::vec3 &position);
    void setCameraRotation(const glm::quat &rotation);
};  