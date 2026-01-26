#include "engine/graphics/backends/diligent/backend.hpp"

#include "platform/window.hpp"

#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace graphics_backend {

DiligentBackend::DiligentBackend(platform::Window& windowIn)
    : window(&windowIn) {
    if (window) {
        window->getFramebufferSize(framebufferWidth, framebufferHeight);
        if (framebufferWidth <= 0) {
            framebufferWidth = 1;
        }
        if (framebufferHeight <= 0) {
            framebufferHeight = 1;
        }
    }
}

DiligentBackend::~DiligentBackend() = default;

void DiligentBackend::beginFrame() {}

void DiligentBackend::endFrame() {}

void DiligentBackend::resize(int width, int height) {
    framebufferWidth = std::max(1, width);
    framebufferHeight = std::max(1, height);
}

graphics::EntityId DiligentBackend::createEntity(graphics::LayerId layer) {
    const graphics::EntityId id = nextEntityId++;
    EntityRecord record;
    record.layer = layer;
    entities[id] = record;
    return id;
}

graphics::EntityId DiligentBackend::createModelEntity(const std::filesystem::path& modelPath,
                                                      graphics::LayerId layer,
                                                      graphics::MaterialId materialOverride) {
    const graphics::EntityId id = createEntity(layer);
    setEntityModel(id, modelPath, materialOverride);
    return id;
}

graphics::EntityId DiligentBackend::createMeshEntity(graphics::MeshId mesh,
                                                     graphics::LayerId layer,
                                                     graphics::MaterialId materialOverride) {
    const graphics::EntityId id = createEntity(layer);
    setEntityMesh(id, mesh, materialOverride);
    return id;
}

void DiligentBackend::setEntityModel(graphics::EntityId entity,
                                     const std::filesystem::path& modelPath,
                                     graphics::MaterialId materialOverride) {
    auto it = entities.find(entity);
    if (it == entities.end()) {
        return;
    }
    it->second.modelPath = modelPath;
    it->second.material = materialOverride;
}

void DiligentBackend::setEntityMesh(graphics::EntityId entity,
                                    graphics::MeshId mesh,
                                    graphics::MaterialId materialOverride) {
    auto it = entities.find(entity);
    if (it == entities.end()) {
        return;
    }
    it->second.mesh = mesh;
    it->second.material = materialOverride;
}

void DiligentBackend::destroyEntity(graphics::EntityId entity) {
    entities.erase(entity);
}

graphics::MeshId DiligentBackend::createMesh(const graphics::MeshData& mesh) {
    const graphics::MeshId id = nextMeshId++;
    meshes[id] = mesh;
    return id;
}

void DiligentBackend::destroyMesh(graphics::MeshId mesh) {
    meshes.erase(mesh);
}

graphics::MaterialId DiligentBackend::createMaterial(const graphics::MaterialDesc& material) {
    const graphics::MaterialId id = nextMaterialId++;
    materials[id] = material;
    return id;
}

void DiligentBackend::updateMaterial(graphics::MaterialId material, const graphics::MaterialDesc& desc) {
    auto it = materials.find(material);
    if (it == materials.end()) {
        return;
    }
    it->second = desc;
}

void DiligentBackend::destroyMaterial(graphics::MaterialId material) {
    materials.erase(material);
}

void DiligentBackend::setMaterialFloat(graphics::MaterialId, std::string_view, float) {
}

graphics::RenderTargetId DiligentBackend::createRenderTarget(const graphics::RenderTargetDesc& desc) {
    const graphics::RenderTargetId id = nextRenderTargetId++;
    RenderTargetRecord record;
    record.desc = desc;
    renderTargets[id] = record;
    return id;
}

void DiligentBackend::destroyRenderTarget(graphics::RenderTargetId target) {
    renderTargets.erase(target);
}

void DiligentBackend::renderLayer(graphics::LayerId, graphics::RenderTargetId) {}

unsigned int DiligentBackend::getRenderTargetTextureId(graphics::RenderTargetId) const {
    return 0u;
}

void DiligentBackend::setPosition(graphics::EntityId entity, const glm::vec3& position) {
    auto it = entities.find(entity);
    if (it == entities.end()) {
        return;
    }
    it->second.position = position;
}

void DiligentBackend::setRotation(graphics::EntityId entity, const glm::quat& rotation) {
    auto it = entities.find(entity);
    if (it == entities.end()) {
        return;
    }
    it->second.rotation = rotation;
}

void DiligentBackend::setScale(graphics::EntityId entity, const glm::vec3& scale) {
    auto it = entities.find(entity);
    if (it == entities.end()) {
        return;
    }
    it->second.scale = scale;
}

void DiligentBackend::setVisible(graphics::EntityId entity, bool visible) {
    auto it = entities.find(entity);
    if (it == entities.end()) {
        return;
    }
    it->second.visible = visible;
}

void DiligentBackend::setTransparency(graphics::EntityId entity, bool transparency) {
    auto it = entities.find(entity);
    if (it == entities.end()) {
        return;
    }
    it->second.transparent = transparency;
}

void DiligentBackend::setCameraPosition(const glm::vec3& position) {
    cameraPosition = position;
}

void DiligentBackend::setCameraRotation(const glm::quat& rotation) {
    cameraRotation = rotation;
}

void DiligentBackend::setPerspective(float fovDeg, float aspect, float nearPlaneIn, float farPlaneIn) {
    usePerspective = true;
    fovDegrees = fovDeg;
    aspectRatio = aspect;
    nearPlane = nearPlaneIn;
    farPlane = farPlaneIn;
}

void DiligentBackend::setOrthographic(float left, float right, float top, float bottom, float nearPlaneIn, float farPlaneIn) {
    usePerspective = false;
    orthoLeft = left;
    orthoRight = right;
    orthoTop = top;
    orthoBottom = bottom;
    nearPlane = nearPlaneIn;
    farPlane = farPlaneIn;
}

glm::mat4 DiligentBackend::computeViewMatrix() const {
    const glm::mat4 rotation = glm::mat4_cast(glm::conjugate(cameraRotation));
    const glm::mat4 translation = glm::translate(glm::mat4(1.0f), -cameraPosition);
    return rotation * translation;
}

glm::mat4 DiligentBackend::computeProjectionMatrix() const {
    if (usePerspective) {
        return glm::perspective(glm::radians(fovDegrees), aspectRatio, nearPlane, farPlane);
    }
    return glm::ortho(orthoLeft, orthoRight, orthoBottom, orthoTop, nearPlane, farPlane);
}

glm::mat4 DiligentBackend::getViewProjectionMatrix() const {
    return computeProjectionMatrix() * computeViewMatrix();
}

glm::mat4 DiligentBackend::getViewMatrix() const {
    return computeViewMatrix();
}

glm::mat4 DiligentBackend::getProjectionMatrix() const {
    return computeProjectionMatrix();
}

glm::vec3 DiligentBackend::getCameraPosition() const {
    return cameraPosition;
}

glm::vec3 DiligentBackend::getCameraForward() const {
    const glm::vec3 forward = cameraRotation * glm::vec3(0.0f, 0.0f, -1.0f);
    return glm::normalize(forward);
}

} // namespace graphics_backend
