#pragma once
#include "renderer/backend.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/mat4x4.hpp>
#include <filesystem>
#include <memory>
#include <string>
#include "core/types.hpp"

#define CAMERA_FOV 60.0f
#define SCREEN_WIDTH 800.0f
#define SCREEN_HEIGHT 600.0f

namespace platform {
class Window;
}

class Render {
    friend class ClientEngine;

private:
    std::unique_ptr<render_backend::Backend> backend_;

    Render(platform::Window &window);
    ~Render();

    void update();
    void resizeCallback(int width, int height);

public:
    render_id create();
    render_id create(std::string modelPath, bool addToRadar = true);
    void setModel(render_id id, const std::filesystem::path& modelPath, bool addToRadar = true);
    void setRadarCircleGraphic(render_id id, float radius = 1.0f);
    void setRadarFOVLinesAngle(float fovDegrees);

    void destroy(render_id id);
    void setPosition(render_id id, const glm::vec3 &position);
    void setRotation(render_id id, const glm::quat &rotation);
    void setScale(render_id id, const glm::vec3 &scale);
    void setVisible(render_id id, bool visible);
    void setTransparency(render_id id, bool transparency);
    void setCameraPosition(const glm::vec3 &position);
    void setCameraRotation(const glm::quat &rotation);

    unsigned int getRadarTextureId() const;
    void setRadarShaderPath(const std::filesystem::path& vertPath,
                            const std::filesystem::path& fragPath);

    // Camera state for downstream systems (e.g., particle renderer).
    glm::mat4 getViewProjectionMatrix() const;
    glm::mat4 getViewMatrix() const;
    glm::mat4 getProjectionMatrix() const;
    glm::vec3 getCameraPosition() const;
    glm::vec3 getCameraForward() const;
};
