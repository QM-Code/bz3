#include "renderer/render.hpp"

#include "platform/window.hpp"
#include "common/data_path_resolver.hpp"
#include "spdlog/spdlog.h"

#include <algorithm>
#include <cmath>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <limits>

namespace {
constexpr float kRadarOrthoHalfSize = 40.0f;
constexpr float kRadarNear = 0.1f;
constexpr float kRadarFar = 500.0f;
constexpr float kRadarHeightAbovePlayer = 60.0f;
constexpr float kRadarBeamLength = 80.0f;
constexpr float kRadarBeamWidth = 0.3f;
constexpr int kRadarTexSize = 512 * 2;

graphics::MeshData makeDiskMesh(int segments = 64, float radius = 1.0f) {
    graphics::MeshData mesh;
    mesh.vertices.reserve(segments + 1);
    mesh.indices.reserve(segments * 3);

    mesh.vertices.emplace_back(0.0f, 0.0f, 0.0f);
    for (int i = 0; i < segments; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(segments) * 2.0f * static_cast<float>(M_PI);
        const float ct = std::cos(t);
        const float st = std::sin(t);
        mesh.vertices.emplace_back(ct * radius, 0.0f, st * radius);
    }

    for (int i = 0; i < segments; ++i) {
        const uint32_t center = 0;
        const uint32_t a = static_cast<uint32_t>(i + 1);
        const uint32_t b = static_cast<uint32_t>(((i + 1) % segments) + 1);
        mesh.indices.insert(mesh.indices.end(), {center, a, b});
    }

    return mesh;
}

graphics::MeshData makeBeamMesh() {
    graphics::MeshData mesh;
    mesh.vertices = {
        {-kRadarBeamWidth * 0.5f, 0.0f, 0.0f},
        { kRadarBeamWidth * 0.5f, 0.0f, 0.0f},
        { kRadarBeamWidth * 0.5f, 0.0f, -1.0f},
        {-kRadarBeamWidth * 0.5f, 0.0f, -1.0f}
    };
    mesh.indices = {0, 1, 2, 0, 2, 3};
    return mesh;
}

} // namespace

Render::Render(platform::Window &windowIn)
    : window(&windowIn) {
    device_ = std::make_unique<graphics::GraphicsDevice>(windowIn);
    ensureRadarResources();

}

Render::~Render() = default;

void Render::resizeCallback(int width, int height) {
    if (device_) {
        device_->resize(width, height);
    }
}

void Render::ensureRadarResources() {
    if (!device_) {
        return;
    }

    if (radarTarget == graphics::kDefaultRenderTarget) {
        graphics::RenderTargetDesc desc;
        desc.width = kRadarTexSize;
        desc.height = kRadarTexSize;
        desc.depth = true;
        desc.stencil = false;
        radarTarget = device_->createRenderTarget(desc);
    }

    if (radarMaterial == graphics::kInvalidMaterial) {
        graphics::MaterialDesc desc;
        desc.transparent = true;
        desc.depthTest = true;
        desc.depthWrite = false;
        desc.doubleSided = true;
        desc.baseColor = {1.0f, 1.0f, 1.0f, 1.0f};
        radarMaterial = device_->createMaterial(desc);
        device_->setMaterialFloat(radarMaterial, "jumpHeight", 5.0f);
    }

    if (radarLineMaterial == graphics::kInvalidMaterial) {
        graphics::MaterialDesc desc;
        desc.unlit = true;
        desc.transparent = true;
        desc.depthTest = false;
        desc.depthWrite = false;
        desc.doubleSided = true;
        desc.baseColor = {1.0f, 1.0f, 1.0f, 1.0f};
        radarLineMaterial = device_->createMaterial(desc);
    }

    if (radarCircleMesh == graphics::kInvalidMesh) {
        radarCircleMesh = device_->createMesh(makeDiskMesh());
    }

    if (radarBeamMesh == graphics::kInvalidMesh) {
        radarBeamMesh = device_->createMesh(makeBeamMesh());
    }
}

void Render::updateRadarFovLines() {
    if (!device_) {
        return;
    }

    ensureRadarResources();

    if (radarFovLeft == graphics::kInvalidEntity) {
        radarFovLeft = device_->createMeshEntity(radarBeamMesh, radarLayer, radarLineMaterial);
    }
    if (radarFovRight == graphics::kInvalidEntity) {
        radarFovRight = device_->createMeshEntity(radarBeamMesh, radarLayer, radarLineMaterial);
    }

    const float halfVertRad = glm::radians(radarFovDegrees * 0.5f);
    const float halfHorizRad = std::atan(std::tan(halfVertRad) * lastAspect);

    const glm::vec3 radarCamPos = cameraPosition + glm::vec3(0.0f, kRadarHeightAbovePlayer, 0.0f);
    const glm::quat radarCamRot = computeRadarCameraRotation(radarCamPos);
    glm::vec3 radarUp = radarCamRot * glm::vec3(0.0f, 1.0f, 0.0f);
    radarUp.y = 0.0f;
    const float len2 = glm::dot(radarUp, radarUp);
    if (len2 < 1e-6f) {
        radarUp = glm::vec3(0.0f, 0.0f, -1.0f);
    } else {
        radarUp *= 1.0f / std::sqrt(len2);
    }
    const float yaw = std::atan2(radarUp.x, -radarUp.z);
    const glm::quat baseYaw = glm::angleAxis(yaw, glm::vec3(0, 1, 0));

    const glm::quat yawLeft = glm::angleAxis(halfHorizRad, glm::vec3(0, 1, 0));
    const glm::quat yawRight = glm::angleAxis(-halfHorizRad, glm::vec3(0, 1, 0));

    const glm::quat leftRot = baseYaw * yawLeft;
    const glm::quat rightRot = baseYaw * yawRight;

    auto maxRayToRadarEdge = [](const glm::vec3& dir) {
        const float half = kRadarOrthoHalfSize;
        const float dx = std::abs(dir.x);
        const float dz = std::abs(dir.z);
        float tx = dx > 1e-6f ? (half / dx) : std::numeric_limits<float>::infinity();
        float tz = dz > 1e-6f ? (half / dz) : std::numeric_limits<float>::infinity();
        return std::min(tx, tz);
    };

    const glm::vec3 leftDir = leftRot * glm::vec3(0.0f, 0.0f, -1.0f);
    const glm::vec3 rightDir = rightRot * glm::vec3(0.0f, 0.0f, -1.0f);
    const float leftLength = maxRayToRadarEdge(leftDir);
    const float rightLength = maxRayToRadarEdge(rightDir);

    device_->setRotation(radarFovLeft, leftRot);
    device_->setPosition(radarFovLeft, cameraPosition);
    device_->setScale(radarFovLeft, glm::vec3(1.0f, 1.0f, leftLength));

    device_->setRotation(radarFovRight, rightRot);
    device_->setPosition(radarFovRight, cameraPosition);
    device_->setScale(radarFovRight, glm::vec3(1.0f, 1.0f, rightLength));
}

glm::quat Render::computeRadarCameraRotation(const glm::vec3& radarCamPos) const {
    glm::vec3 forward = glm::mat3_cast(cameraRotation) * glm::vec3(0.0f, 0.0f, -1.0f);
    forward.y = 0.0f;
    float len2 = glm::dot(forward, forward);
    if (len2 < 1e-6f) {
        forward = glm::vec3(0.0f, 0.0f, -1.0f);
    } else {
        forward *= 1.0f / std::sqrt(len2);
    }

    // Match the old behavior: camera looks at the player with "up" pointing along player forward.
    const glm::vec3 up = forward;
    const glm::mat4 view = glm::lookAt(radarCamPos, cameraPosition, up);
    const glm::mat4 world = glm::inverse(view);
    return glm::quat_cast(glm::mat3(world));
}

void Render::update() {
    if (!device_) {
        return;
    }

    int width = 0;
    int height = 0;
    if (window) {
        window->getFramebufferSize(width, height);
    }
    if (height <= 0) {
        height = 1;
    }
    if (width <= 0) {
        width = 1;
    }

    lastAspect = static_cast<float>(width) / static_cast<float>(height);
    device_->resize(width, height);

    device_->beginFrame();

    ensureRadarResources();
    updateRadarFovLines();

    // Render radar layer to offscreen target
    device_->setOrthographic(-kRadarOrthoHalfSize, kRadarOrthoHalfSize,
                             kRadarOrthoHalfSize, -kRadarOrthoHalfSize,
                             kRadarNear, kRadarFar);
    const glm::vec3 radarCamPos = cameraPosition + glm::vec3(0.0f, kRadarHeightAbovePlayer, 0.0f);
    device_->setCameraPosition(radarCamPos);
    device_->setCameraRotation(computeRadarCameraRotation(radarCamPos));
    device_->setMaterialFloat(radarMaterial, "playerY", cameraPosition.y);
    device_->renderLayer(radarLayer, radarTarget);

    // Debug: track radar render target texture id changes.
    {
        static unsigned int lastRadarTex = ~0u;
        const unsigned int currentRadarTex = device_->getRenderTargetTextureId(radarTarget);
        if (currentRadarTex != lastRadarTex) {
            spdlog::info("Radar RT texture id changed: {} -> {} (entities: {}, circles: {})",
                         lastRadarTex, currentRadarTex, radarEntities.size(), radarCircles.size());
            lastRadarTex = currentRadarTex;
        }
    }
    {
        static size_t lastEntities = static_cast<size_t>(-1);
        static size_t lastCircles = static_cast<size_t>(-1);
        if (radarEntities.size() != lastEntities || radarCircles.size() != lastCircles) {
            lastEntities = radarEntities.size();
            lastCircles = radarCircles.size();
        }
    }

    // Render main layer to screen
    device_->setPerspective(CAMERA_FOV, lastAspect, 0.1f, 1000.0f);
    device_->setCameraPosition(cameraPosition);
    device_->setCameraRotation(cameraRotation);
    device_->renderLayer(mainLayer, graphics::kDefaultRenderTarget);

}

render_id Render::create() {
    const render_id id = nextId++;
    const auto entity = device_->createEntity(mainLayer);
    mainEntities[id] = entity;
    return id;
}

render_id Render::create(std::string modelPath, bool addToRadar) {
    const render_id id = nextId++;
    const auto entity = device_->createModelEntity(modelPath, mainLayer, graphics::kInvalidMaterial);
    mainEntities[id] = entity;
    modelPaths[id] = modelPath;

    if (addToRadar) {
        ensureRadarResources();
        const auto radarEntity = device_->createModelEntity(modelPath, radarLayer, radarMaterial);
        radarEntities[id] = radarEntity;
    }

    return id;
}

void Render::setModel(render_id id, const std::filesystem::path& modelPath, bool addToRadar) {
    auto it = mainEntities.find(id);
    if (it != mainEntities.end()) {
        device_->setEntityModel(it->second, modelPath, graphics::kInvalidMaterial);
    }
    modelPaths[id] = modelPath;

    if (addToRadar) {
        ensureRadarResources();
        auto rit = radarEntities.find(id);
        if (rit == radarEntities.end()) {
            const auto radarEntity = device_->createModelEntity(modelPath, radarLayer, radarMaterial);
            radarEntities[id] = radarEntity;
        } else {
            device_->setEntityModel(rit->second, modelPath, radarMaterial);
        }
    } else {
        auto rit = radarEntities.find(id);
        if (rit != radarEntities.end()) {
            device_->destroyEntity(rit->second);
            radarEntities.erase(rit);
        }
    }
}

void Render::setRadarCircleGraphic(render_id id, float radius) {
    ensureRadarResources();

    auto it = radarCircles.find(id);
    if (it == radarCircles.end()) {
        const auto circleEntity = device_->createMeshEntity(radarCircleMesh, radarLayer, radarLineMaterial);
        radarCircles[id] = circleEntity;
        it = radarCircles.find(id);
    }

    device_->setScale(it->second, glm::vec3(radius, 1.0f, radius));
}

void Render::setRadarFOVLinesAngle(float fovDegrees) {
    radarFovDegrees = fovDegrees;
    updateRadarFovLines();
}

void Render::destroy(render_id id) {
    auto it = mainEntities.find(id);
    if (it != mainEntities.end()) {
        device_->destroyEntity(it->second);
        mainEntities.erase(it);
    }

    auto rit = radarEntities.find(id);
    if (rit != radarEntities.end()) {
        device_->destroyEntity(rit->second);
        radarEntities.erase(rit);
    }

    auto cit = radarCircles.find(id);
    if (cit != radarCircles.end()) {
        device_->destroyEntity(cit->second);
        radarCircles.erase(cit);
    }

    modelPaths.erase(id);
}

void Render::setPosition(render_id id, const glm::vec3 &position) {
    if (auto it = mainEntities.find(id); it != mainEntities.end()) {
        device_->setPosition(it->second, position);
    }
    if (auto it = radarEntities.find(id); it != radarEntities.end()) {
        device_->setPosition(it->second, position);
    }
    if (auto it = radarCircles.find(id); it != radarCircles.end()) {
        device_->setPosition(it->second, position);
    }
}

void Render::setRotation(render_id id, const glm::quat &rotation) {
    if (auto it = mainEntities.find(id); it != mainEntities.end()) {
        device_->setRotation(it->second, rotation);
    }
    if (auto it = radarEntities.find(id); it != radarEntities.end()) {
        device_->setRotation(it->second, rotation);
    }
    if (auto it = radarCircles.find(id); it != radarCircles.end()) {
        device_->setRotation(it->second, rotation);
    }
}

void Render::setScale(render_id id, const glm::vec3 &scale) {
    if (auto it = mainEntities.find(id); it != mainEntities.end()) {
        device_->setScale(it->second, scale);
    }
    if (auto it = radarEntities.find(id); it != radarEntities.end()) {
        device_->setScale(it->second, scale);
    }
}

void Render::setVisible(render_id id, bool visible) {
    if (auto it = mainEntities.find(id); it != mainEntities.end()) {
        device_->setVisible(it->second, visible);
    }
    if (auto it = radarEntities.find(id); it != radarEntities.end()) {
        device_->setVisible(it->second, visible);
    }
    if (auto it = radarCircles.find(id); it != radarCircles.end()) {
        device_->setVisible(it->second, visible);
    }
}

void Render::setTransparency(render_id id, bool transparency) {
    if (auto it = mainEntities.find(id); it != mainEntities.end()) {
        device_->setTransparency(it->second, transparency);
    }
}

void Render::setCameraPosition(const glm::vec3 &position) {
    cameraPosition = position;
    if (device_) {
        device_->setCameraPosition(position);
    }
    updateRadarFovLines();
}

void Render::setCameraRotation(const glm::quat &rotation) {
    cameraRotation = rotation;
    if (device_) {
        device_->setCameraRotation(rotation);
    }
    updateRadarFovLines();
}

graphics::TextureHandle Render::getRadarTexture() const {
    graphics::TextureHandle handle{};
    if (!device_) {
        return handle;
    }
    const unsigned int textureId = device_->getRenderTargetTextureId(radarTarget);
    handle.id = static_cast<uint64_t>(textureId);
    handle.width = static_cast<uint32_t>(kRadarTexSize);
    handle.height = static_cast<uint32_t>(kRadarTexSize);
    return handle;
}

graphics_backend::UiRenderTargetBridge* Render::getUiRenderTargetBridge() const {
    return device_ ? device_->getUiRenderTargetBridge() : nullptr;
}

void Render::setUiOverlayTexture(const ui::RenderOutput& output) {
    if (!device_) {
        return;
    }
    device_->setUiOverlayTexture(output.texture);
    device_->setUiOverlayVisible(output.valid());
}

void Render::renderUiOverlay() {
    if (!device_) {
        return;
    }
    device_->renderUiOverlay();
}

void Render::setBrightness(float brightness) {
    if (!device_) {
        return;
    }
    device_->setBrightness(brightness);
}

void Render::present() {
    if (!device_) {
        return;
    }
    device_->endFrame();
}

void Render::setRadarShaderPath(const std::filesystem::path& vertPath,
                                const std::filesystem::path& fragPath) {
    ensureRadarResources();
    graphics::MaterialDesc desc;
    desc.vertexShaderPath = vertPath;
    desc.fragmentShaderPath = fragPath;
    desc.transparent = true;
    desc.depthTest = true;
    desc.depthWrite = false;
    desc.doubleSided = true;
    desc.baseColor = {1.0f, 1.0f, 1.0f, 1.0f};
    if (radarMaterial == graphics::kInvalidMaterial) {
        radarMaterial = device_->createMaterial(desc);
    } else {
        device_->updateMaterial(radarMaterial, desc);
    }
    device_->setMaterialFloat(radarMaterial, "jumpHeight", 5.0f);
    device_->setMaterialFloat(radarMaterial, "playerY", cameraPosition.y);

    for (const auto& [id, entity] : radarEntities) {
        auto pathIt = modelPaths.find(id);
        if (pathIt != modelPaths.end()) {
            device_->setEntityModel(entity, pathIt->second, radarMaterial);
        }
    }
}

glm::mat4 Render::getViewProjectionMatrix() const {
    return device_ ? device_->getViewProjectionMatrix() : glm::mat4(1.0f);
}

glm::mat4 Render::getViewMatrix() const {
    return device_ ? device_->getViewMatrix() : glm::mat4(1.0f);
}

glm::mat4 Render::getProjectionMatrix() const {
    return device_ ? device_->getProjectionMatrix() : glm::mat4(1.0f);
}

glm::vec3 Render::getCameraPosition() const {
    return device_ ? device_->getCameraPosition() : glm::vec3(0.0f);
}

glm::vec3 Render::getCameraForward() const {
    return device_ ? device_->getCameraForward() : glm::vec3(0.0f, 0.0f, -1.0f);
}
