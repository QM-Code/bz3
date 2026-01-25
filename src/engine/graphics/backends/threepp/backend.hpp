#pragma once

#include "engine/graphics/backend.hpp"

#include <threepp/threepp.hpp>
#include <threepp/cameras/OrthographicCamera.hpp>
#include <threepp/renderers/GLRenderTarget.hpp>
#include <glm/gtc/quaternion.hpp>
#include <memory>
#include <unordered_map>

namespace threepp {
class MeshStandardMaterial;
}

namespace graphics_backend {

class ThreeppBackend final : public Backend {
public:
    explicit ThreeppBackend(platform::Window& window);
    ~ThreeppBackend() override;

    void beginFrame() override;
    void endFrame() override;
    void resize(int width, int height) override;

    graphics::EntityId createEntity(graphics::LayerId layer) override;
    graphics::EntityId createModelEntity(const std::filesystem::path& modelPath,
                                         graphics::LayerId layer,
                                         graphics::MaterialId materialOverride) override;
    graphics::EntityId createMeshEntity(graphics::MeshId mesh,
                                        graphics::LayerId layer,
                                        graphics::MaterialId materialOverride) override;
    void setEntityModel(graphics::EntityId entity,
                        const std::filesystem::path& modelPath,
                        graphics::MaterialId materialOverride) override;
    void setEntityMesh(graphics::EntityId entity,
                       graphics::MeshId mesh,
                       graphics::MaterialId materialOverride) override;
    void destroyEntity(graphics::EntityId entity) override;

    graphics::MeshId createMesh(const graphics::MeshData& mesh) override;
    void destroyMesh(graphics::MeshId mesh) override;

    graphics::MaterialId createMaterial(const graphics::MaterialDesc& material) override;
    void updateMaterial(graphics::MaterialId material, const graphics::MaterialDesc& desc) override;
    void destroyMaterial(graphics::MaterialId material) override;
    void setMaterialFloat(graphics::MaterialId material, std::string_view name, float value) override;

    graphics::RenderTargetId createRenderTarget(const graphics::RenderTargetDesc& desc) override;
    void destroyRenderTarget(graphics::RenderTargetId target) override;

    void renderLayer(graphics::LayerId layer, graphics::RenderTargetId target) override;

    unsigned int getRenderTargetTextureId(graphics::RenderTargetId target) const override;

    void setPosition(graphics::EntityId entity, const glm::vec3& position) override;
    void setRotation(graphics::EntityId entity, const glm::quat& rotation) override;
    void setScale(graphics::EntityId entity, const glm::vec3& scale) override;
    void setVisible(graphics::EntityId entity, bool visible) override;
    void setTransparency(graphics::EntityId entity, bool transparency) override;

    void setCameraPosition(const glm::vec3& position) override;
    void setCameraRotation(const glm::quat& rotation) override;
    void setPerspective(float fovDegrees, float aspect, float nearPlane, float farPlane) override;
    void setOrthographic(float left, float right, float top, float bottom, float nearPlane, float farPlane) override;

    glm::mat4 getViewProjectionMatrix() const override;
    glm::mat4 getViewMatrix() const override;
    glm::mat4 getProjectionMatrix() const override;
    glm::vec3 getCameraPosition() const override;
    glm::vec3 getCameraForward() const override;

private:
    struct EntityRecord {
        graphics::LayerId layer = 0;
        std::shared_ptr<threepp::Object3D> object;
    };

    std::shared_ptr<threepp::Scene> sceneForLayer(graphics::LayerId layer);
    std::shared_ptr<threepp::Material> materialForId(graphics::MaterialId material) const;
    std::shared_ptr<threepp::Material> createMaterialInstance(const graphics::MaterialDesc& desc) const;

    platform::Window* window = nullptr;
    threepp::GLRenderer renderer;
    std::shared_ptr<threepp::PerspectiveCamera> perspectiveCamera_;
    std::shared_ptr<threepp::OrthographicCamera> orthoCamera_;
    std::shared_ptr<threepp::Camera> activeCamera_;

    graphics::EntityId nextEntityId = 1;
    graphics::MeshId nextMeshId = 1;
    graphics::MaterialId nextMaterialId = 1;
    graphics::RenderTargetId nextTargetId = 1;

    std::unordered_map<graphics::LayerId, std::shared_ptr<threepp::Scene>> scenes_;
    std::unordered_map<graphics::EntityId, EntityRecord> entities_;
    std::unordered_map<graphics::MeshId, std::shared_ptr<threepp::BufferGeometry>> meshes_;
    std::unordered_map<graphics::MaterialId, std::shared_ptr<threepp::Material>> materials_;
    std::unordered_map<graphics::RenderTargetId, std::unique_ptr<threepp::GLRenderTarget>> targets_;
    std::shared_ptr<threepp::MeshStandardMaterial> defaultMaterial_;
};

} // namespace graphics_backend
