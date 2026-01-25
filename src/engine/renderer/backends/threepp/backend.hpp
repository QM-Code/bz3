#pragma once

#include "renderer/backend.hpp"
#include <threepp/threepp.hpp>
#include <threepp/cameras/OrthographicCamera.hpp>
#include <threepp/objects/Line.hpp>
#include <threepp/objects/Mesh.hpp>
#include <threepp/renderers/GLRenderTarget.hpp>
#include <functional>
#include <map>
#include <memory>

namespace threepp {
class ShaderMaterial;
}

namespace render_backend {

class ThreeppBackend final : public Backend {
public:
    explicit ThreeppBackend(platform::Window& window);
    ~ThreeppBackend() override;

    void update() override;
    void resizeCallback(int width, int height) override;

    render_id create() override;
    render_id create(std::string modelPath, bool addToRadar) override;
    void setModel(render_id id, const std::filesystem::path& modelPath, bool addToRadar) override;
    void setRadarCircleGraphic(render_id id, float radius) override;
    void setRadarFOVLinesAngle(float fovDegrees) override;

    void destroy(render_id id) override;
    void setPosition(render_id id, const glm::vec3& position) override;
    void setRotation(render_id id, const glm::quat& rotation) override;
    void setScale(render_id id, const glm::vec3& scale) override;
    void setVisible(render_id id, bool visible) override;
    void setTransparency(render_id id, bool transparency) override;
    void setCameraPosition(const glm::vec3& position) override;
    void setCameraRotation(const glm::quat& rotation) override;

    unsigned int getRadarTextureId() const override { return radarTextureId; }
    void setRadarShaderPath(const std::filesystem::path& vertPath,
                            const std::filesystem::path& fragPath) override;

    glm::mat4 getViewProjectionMatrix() const override;
    glm::mat4 getViewMatrix() const override;
    glm::mat4 getProjectionMatrix() const override;
    glm::vec3 getCameraPosition() const override;
    glm::vec3 getCameraForward() const override;

private:
    platform::Window* window = nullptr;
    threepp::GLRenderer renderer;
    std::shared_ptr<threepp::Scene> scene;
    std::shared_ptr<threepp::Scene> radarScene;
    std::shared_ptr<threepp::PerspectiveCamera> camera;

    std::shared_ptr<threepp::ShaderMaterial> radarMaterial;

    std::shared_ptr<threepp::Mesh> radarFOVLeft;
    std::shared_ptr<threepp::Mesh> radarFOVRight;
    float radarFOVBeamLength = 80.0f;
    float radarFOVBeamWidth = 0.3f;

    std::shared_ptr<threepp::OrthographicCamera> radarCamera;
    std::unique_ptr<threepp::GLRenderTarget> radarRenderTarget;
    unsigned int radarTextureId = 0;
    glm::vec3 radarAnchorPosition{0.0f};
    glm::quat radarAnchorRotation{1.0f, 0.0f, 0.0f, 0.0f};

    std::map<render_id, std::shared_ptr<threepp::Group>> objects;
    std::map<render_id, std::shared_ptr<threepp::Group>> radarObjects;

    void ensureRadarMaterialLoaded();
};

} // namespace render_backend
