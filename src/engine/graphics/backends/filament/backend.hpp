#pragma once

#include "engine/graphics/backend.hpp"

#include <filament/Camera.h>
#include <filament/Engine.h>
#include <filament/IndexBuffer.h>
#include <filament/Material.h>
#include <filament/MaterialInstance.h>
#include <filament/RenderableManager.h>
#include <filament/Renderer.h>
#include <filament/RenderTarget.h>
#include <filament/Scene.h>
#include <filament/SwapChain.h>
#include <filament/Texture.h>
#include <filament/VertexBuffer.h>
#include <filament/View.h>
#include <filament/LightManager.h>
#include <filament/IndirectLight.h>
#include <filament/Skybox.h>
#include <gltfio/AssetLoader.h>
#include <gltfio/FilamentAsset.h>
#include <gltfio/MaterialProvider.h>
#include <gltfio/ResourceLoader.h>
#include <gltfio/TextureProvider.h>
#include <utils/EntityManager.h>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <memory>
#include <unordered_map>

struct wl_display;
struct wl_surface;

namespace filament::backend {
class Platform;
}

namespace graphics_backend {

enum class FilamentBackendPreference {
    OpenGL,
    Vulkan,
};

void SetFilamentBackendPreference(FilamentBackendPreference preference);

namespace filament_backend_detail {
struct WaylandNativeWindow {
    wl_display* display = nullptr;
    wl_surface* surface = nullptr;
    uint32_t width = 0;
    uint32_t height = 0;
};
}

class FilamentBackend final : public Backend {
public:
    explicit FilamentBackend(platform::Window& window);
    ~FilamentBackend() override;

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
    void setUiOverlayTexture(const graphics::TextureHandle& texture) override;
    void setUiOverlayVisible(bool visible) override;
    void renderUiOverlay() override;
    void setBrightness(float brightness) override;

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
        utils::Entity entity;
        glm::vec3 position{0.0f};
        glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
        glm::vec3 scale{1.0f};
        bool visible = true;
    };

    struct LayerState {
        filament::Scene* scene = nullptr;
        filament::View* view = nullptr;
    };

    struct RenderTargetRecord {
        graphics::RenderTargetDesc desc;
        filament::RenderTarget* target = nullptr;
        filament::Texture* colorTexture = nullptr;
        filament::Texture* depthTexture = nullptr;
        unsigned int colorTextureId = 0;
        unsigned int depthTextureId = 0;
    };

    void ensureLayer(graphics::LayerId layer);
    void applyTransform(const EntityRecord& record);
    void updateViewMatrix();
    void updateProjectionMatrix();
    void ensureSceneLighting(graphics::LayerId layer);
    void destroyAsset(graphics::EntityId id);

    platform::Window* window = nullptr;
    filament::Engine* engine = nullptr;
    filament::Renderer* renderer = nullptr;
    filament::View* uiView = nullptr;
    filament::Scene* uiScene = nullptr;
    filament::Camera* uiCamera = nullptr;
    utils::Entity uiCameraEntity;
    utils::Entity uiQuadEntity;
    filament::VertexBuffer* uiVertexBuffer = nullptr;
    filament::IndexBuffer* uiIndexBuffer = nullptr;
    filament::MaterialInstance* uiMaterialInstance = nullptr;
    filament::Texture* uiTexture = nullptr;
    filament::TextureSampler uiSampler;
    unsigned int uiTextureId = 0;
    int uiTextureWidth = 0;
    int uiTextureHeight = 0;
    bool uiVisible = false;
    bool uiInScene = false;
    filament::SwapChain* swapChain = nullptr;
    void* nativeSwapChainHandle = nullptr;
    filament_backend_detail::WaylandNativeWindow* waylandWindow = nullptr;
    filament::backend::Platform* customPlatform = nullptr;
    bool swapChainIsNative = false;
    filament::Camera* camera = nullptr;
    utils::Entity cameraEntity;
    utils::Entity lightEntity;
    utils::Entity ambientEntity;
    bool lightInitialized = false;
    filament::IndirectLight* indirectLight = nullptr;
    filament::Skybox* skybox = nullptr;
    filament::Texture* iblTexture = nullptr;
    filament::Texture* skyboxTexture = nullptr;
    bool iblInitialized = false;
    float brightness = 1.0f;
    float keyLightBaseIntensity = 60000.0f;
    float fillLightBaseIntensity = 40000.0f;
    float iblBaseIntensity = 30000.0f;

    bool frameActive = false;
    int framebufferWidth = 1;
    int framebufferHeight = 1;

    glm::vec3 cameraPosition{0.0f};
    glm::quat cameraRotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::mat4 projectionMatrix{1.0f};
    glm::mat4 viewMatrix{1.0f};

    float lastFovDegrees = 60.0f;
    float lastAspect = 1.0f;
    float lastNear = 0.1f;
    float lastFar = 1000.0f;
    bool lastProjectionWasOrtho = false;
    float lastOrthoLeft = -1.0f;
    float lastOrthoRight = 1.0f;
    float lastOrthoTop = 1.0f;
    float lastOrthoBottom = -1.0f;

    graphics::EntityId nextEntityId = 1;
    graphics::MeshId nextMeshId = 1;
    graphics::MaterialId nextMaterialId = 1;
    graphics::RenderTargetId nextTargetId = 1;

    std::unordered_map<graphics::LayerId, LayerState> layers;
    std::unordered_map<graphics::EntityId, EntityRecord> entities;
    std::unordered_map<graphics::MeshId, graphics::MeshData> meshes;
    std::unordered_map<graphics::MaterialId, graphics::MaterialDesc> materials;
    std::unordered_map<graphics::RenderTargetId, RenderTargetRecord> renderTargets;
    std::unordered_map<graphics::EntityId, std::filesystem::path> modelPaths;
    std::unordered_map<graphics::EntityId, filament::gltfio::FilamentAsset*> assets;

    filament::gltfio::MaterialProvider* materialProvider = nullptr;
    filament::gltfio::TextureProvider* textureProvider = nullptr;
    filament::gltfio::AssetLoader* assetLoader = nullptr;

    mutable bool warnedMissingTargets = false;
    mutable bool warnedRenderTargets = false;
};

} // namespace graphics_backend
