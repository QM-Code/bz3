#pragma once

#include "engine/graphics/backend.hpp"
#if defined(BZ3_UI_BACKEND_IMGUI)
#include "ui/frontends/imgui/platform/renderer_forge.hpp"
#endif

#include <unordered_map>
#include <vector>

#include "Common_3/Graphics/Interfaces/IGraphics.h"

#ifdef Button4
#undef Button4
#endif
#ifdef Button5
#undef Button5
#endif
#ifdef Button6
#undef Button6
#endif
#ifdef Button7
#undef Button7
#endif
#ifdef Button8
#undef Button8
#endif
#ifdef Key
#undef Key
#endif
#ifdef assume
#undef assume
#endif

namespace graphics_backend {

class ForgeBackend final : public Backend {
public:
    explicit ForgeBackend(platform::Window& window);
    ~ForgeBackend() override;

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
    UiRenderTargetBridge* getUiRenderTargetBridge() override { return uiBridge_.get(); }
    const UiRenderTargetBridge* getUiRenderTargetBridge() const override { return uiBridge_.get(); }

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
        glm::vec3 position{0.0f};
        glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
        glm::vec3 scale{1.0f};
        bool visible = true;
        bool transparent = false;
        graphics::MeshId mesh = graphics::kInvalidMesh;
        std::vector<graphics::MeshId> meshes;
        graphics::MaterialId material = graphics::kInvalidMaterial;
        std::filesystem::path modelPath;
    };

    struct MeshRecord {
        Buffer* vertexBuffer = nullptr;
        Buffer* indexBuffer = nullptr;
        uint32_t indexCount = 0;
        Texture* texture = nullptr;
    };

    struct RenderTargetRecord {
        graphics::RenderTargetDesc desc{};
        RenderTarget* renderTarget = nullptr;
        uint64_t token = 0;
    };

    platform::Window* window = nullptr;
    int framebufferWidth = 0;
    int framebufferHeight = 0;

    graphics::EntityId nextEntityId = 1;
    graphics::MeshId nextMeshId = 1;
    graphics::MaterialId nextMaterialId = 1;
    graphics::RenderTargetId nextRenderTargetId = 1;

    std::unordered_map<graphics::EntityId, EntityRecord> entities;
    std::unordered_map<graphics::MeshId, MeshRecord> meshes;
    std::unordered_map<std::string, std::vector<graphics::MeshId>> modelMeshCache;
    std::unordered_map<graphics::MaterialId, graphics::MaterialDesc> materials;
    std::unordered_map<graphics::RenderTargetId, RenderTargetRecord> renderTargets;

    graphics::TextureHandle uiOverlayTexture_{};
    bool uiOverlayVisible_ = false;

    float brightness_ = 1.0f;
    RenderTarget* sceneTarget_ = nullptr;
    int sceneTargetWidth_ = 0;
    int sceneTargetHeight_ = 0;
    Shader* brightnessShader_ = nullptr;
    Pipeline* brightnessPipeline_ = nullptr;
    DescriptorSet* brightnessDescriptorSet_ = nullptr;
    Buffer* brightnessVertexBuffer_ = nullptr;
    Buffer* brightnessIndexBuffer_ = nullptr;
    Buffer* brightnessUniformBuffer_ = nullptr;
    Descriptor brightnessDescriptors_[3]{};
    Sampler* brightnessSampler_ = nullptr;

    Renderer* renderer_ = nullptr;
    Queue* graphicsQueue_ = nullptr;
    SwapChain* swapChain_ = nullptr;
    Fence* renderFence_ = nullptr;
    Semaphore* imageAcquiredSemaphore_ = nullptr;
    Semaphore* renderCompleteSemaphore_ = nullptr;
    CmdPool* cmdPool_ = nullptr;
    Cmd* cmd_ = nullptr;
    uint32_t frameIndex_ = 0;

    glm::vec3 cameraPosition{0.0f};
    glm::quat cameraRotation{1.0f, 0.0f, 0.0f, 0.0f};
    bool usePerspective = true;
    float fovDegrees = 60.0f;
    float aspectRatio = 1.0f;
    float nearPlane = 0.1f;
    float farPlane = 1000.0f;
    float orthoLeft = -1.0f;
    float orthoRight = 1.0f;
    float orthoTop = 1.0f;
    float orthoBottom = -1.0f;

    glm::mat4 computeViewMatrix() const;
    glm::mat4 computeProjectionMatrix() const;

    void ensureUiOverlayResources();
    void destroyUiOverlayResources();
    void ensureDebugTriangleTexture();
    void ensureBrightnessResources();
    void destroyBrightnessResources();
    void renderDebugTriangle();
    void ensureSceneTarget(int width, int height);
    void destroySceneTarget();
    void renderBrightnessPass();
    void ensureMeshResources();
    void destroyMeshResources();

    std::unique_ptr<UiRenderTargetBridge> uiBridge_;

    Shader* uiOverlayShader_ = nullptr;
    Pipeline* uiOverlayPipeline_ = nullptr;
    DescriptorSet* uiOverlayDescriptorSet_ = nullptr;
    Buffer* uiOverlayVertexBuffer_ = nullptr;
    Buffer* uiOverlayIndexBuffer_ = nullptr;
    Buffer* uiOverlayUniformBuffer_ = nullptr;
    Descriptor uiOverlayDescriptors_[3]{};
    Sampler* uiOverlaySampler_ = nullptr;
    Texture* debugTriangleTexture_ = nullptr;
    bool debugTriangleEnabled_ = false;

    Shader* meshShader_ = nullptr;
    Pipeline* meshPipeline_ = nullptr;
    Pipeline* meshPipelineOffscreen_ = nullptr;
    DescriptorSet* meshDescriptorSet_ = nullptr;
    Buffer* meshUniformBuffer_ = nullptr;
    Texture* whiteTexture_ = nullptr;
    Sampler* meshSampler_ = nullptr;
    Descriptor meshDescriptors_[3]{};
};

} // namespace graphics_backend
