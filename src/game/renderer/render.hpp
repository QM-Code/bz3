#pragma once

#include "engine/graphics/device.hpp"
#include "core/types.hpp"

#include <filesystem>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <memory>
#include <string>
#include <unordered_map>

#define CAMERA_FOV 60.0f
#define SCREEN_WIDTH 800.0f
#define SCREEN_HEIGHT 600.0f

namespace platform {
class Window;
}

class Render {
    friend class ClientEngine;

private:
    std::unique_ptr<graphics::GraphicsDevice> device_;
    platform::Window* window = nullptr;

    render_id nextId = 1;

    graphics::LayerId mainLayer = 0;
    graphics::LayerId radarLayer = 1;

    graphics::RenderTargetId radarTarget = graphics::kDefaultRenderTarget;
    graphics::MaterialId radarMaterial = graphics::kInvalidMaterial;
    graphics::MaterialId radarLineMaterial = graphics::kInvalidMaterial;

    graphics::MeshId radarCircleMesh = graphics::kInvalidMesh;
    graphics::MeshId radarBeamMesh = graphics::kInvalidMesh;

    graphics::EntityId radarFovLeft = graphics::kInvalidEntity;
    graphics::EntityId radarFovRight = graphics::kInvalidEntity;

    std::unordered_map<render_id, graphics::EntityId> mainEntities;
    std::unordered_map<render_id, graphics::EntityId> radarEntities;
    std::unordered_map<render_id, graphics::EntityId> radarCircles;
    std::unordered_map<render_id, std::filesystem::path> modelPaths;

    glm::vec3 cameraPosition{0.0f};
    glm::quat cameraRotation{1.0f, 0.0f, 0.0f, 0.0f};

    float lastAspect = 1.0f;
    float radarFovDegrees = CAMERA_FOV;

    Render(platform::Window &window);
    ~Render();

    void update();
    void resizeCallback(int width, int height);

    void ensureRadarResources();
    void updateRadarFovLines();
    glm::quat computeRadarCameraRotation(const glm::vec3& radarCamPos) const;

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

    glm::mat4 getViewProjectionMatrix() const;
    glm::mat4 getViewMatrix() const;
    glm::mat4 getProjectionMatrix() const;
    glm::vec3 getCameraPosition() const;
    glm::vec3 getCameraForward() const;
};
