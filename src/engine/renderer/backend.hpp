#pragma once

#include "core/types.hpp"
#include <filesystem>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/mat4x4.hpp>
#include <memory>
#include <string>

namespace platform {
class Window;
}

namespace render_backend {

class Backend {
public:
    virtual ~Backend() = default;

    virtual void update() = 0;
    virtual void resizeCallback(int width, int height) = 0;

    virtual render_id create() = 0;
    virtual render_id create(std::string modelPath, bool addToRadar) = 0;
    virtual void setModel(render_id id, const std::filesystem::path& modelPath, bool addToRadar) = 0;
    virtual void setRadarCircleGraphic(render_id id, float radius) = 0;
    virtual void setRadarFOVLinesAngle(float fovDegrees) = 0;

    virtual void destroy(render_id id) = 0;
    virtual void setPosition(render_id id, const glm::vec3& position) = 0;
    virtual void setRotation(render_id id, const glm::quat& rotation) = 0;
    virtual void setScale(render_id id, const glm::vec3& scale) = 0;
    virtual void setVisible(render_id id, bool visible) = 0;
    virtual void setTransparency(render_id id, bool transparency) = 0;
    virtual void setCameraPosition(const glm::vec3& position) = 0;
    virtual void setCameraRotation(const glm::quat& rotation) = 0;

    virtual unsigned int getRadarTextureId() const = 0;
    virtual void setRadarShaderPath(const std::filesystem::path& vertPath,
                                    const std::filesystem::path& fragPath) = 0;

    virtual glm::mat4 getViewProjectionMatrix() const = 0;
    virtual glm::mat4 getViewMatrix() const = 0;
    virtual glm::mat4 getProjectionMatrix() const = 0;
    virtual glm::vec3 getCameraPosition() const = 0;
    virtual glm::vec3 getCameraForward() const = 0;
};

std::unique_ptr<Backend> CreateRenderBackend(platform::Window& window);

} // namespace render_backend
