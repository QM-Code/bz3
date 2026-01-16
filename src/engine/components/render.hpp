#pragma once
#include <threepp/threepp.hpp>
#include <threepp/cameras/OrthographicCamera.hpp>
#include <threepp/renderers/GLRenderTarget.hpp>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/mat4x4.hpp>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <map>
#include "engine/types.hpp"

#define CAMERA_FOV 60.0f
#define SCREEN_WIDTH 800.0f
#define SCREEN_HEIGHT 600.0f

namespace threepp {
class ShaderMaterial;
}

class Render {
    friend class ClientEngine;

private:
    // Scene
    GLFWwindow *window;
    threepp::GLRenderer renderer;
    std::shared_ptr<threepp::Scene> scene;
    std::shared_ptr<threepp::Scene> radarScene;
    std::shared_ptr<threepp::PerspectiveCamera> camera;

    std::shared_ptr<threepp::ShaderMaterial> radarMaterial;
    std::function<std::filesystem::path(const std::string&)> assetPathResolver;

    void ensureRadarMaterialLoaded();

    // Radar camera rendering
    std::shared_ptr<threepp::OrthographicCamera> radarCamera;
    std::unique_ptr<threepp::GLRenderTarget> radarRenderTarget;
    unsigned int radarTextureId = 0;
    glm::vec3 radarAnchorPosition{0.0f};
    glm::quat radarAnchorRotation{1.0f, 0.0f, 0.0f, 0.0f};

    std::map<render_id, std::shared_ptr<threepp::Group>> objects;
    std::map<render_id, std::shared_ptr<threepp::Group>> radarObjects;

    Render(GLFWwindow *window);
    ~Render();

    void update();
    void resizeCallback(int width, int height);

public:
    render_id create(std::string modelPath, float radius = 0.0f);
    void destroy(render_id id);
    void setPosition(render_id id, const glm::vec3 &position);
    void setRotation(render_id id, const glm::quat &rotation);
    void setScale(render_id id, const glm::vec3 &scale);
    void setVisible(render_id id, bool visible);
    void setTransparency(render_id id, bool transparency);
    void setCameraPosition(const glm::vec3 &position);
    void setCameraRotation(const glm::quat &rotation);

    unsigned int getRadarTextureId() const { return radarTextureId; }
    void setRadarShaderPath(const std::filesystem::path& vertPath,
                            const std::filesystem::path& fragPath);

    // Camera state for downstream systems (e.g., particle renderer).
    glm::mat4 getViewProjectionMatrix() const;
    glm::mat4 getViewMatrix() const;
    glm::mat4 getProjectionMatrix() const;
    glm::vec3 getCameraPosition() const;
    glm::vec3 getCameraForward() const;
};  