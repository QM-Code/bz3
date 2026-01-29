/*
 * RmlUi Forge renderer stub for BZ3.
 * TODO: Implement actual Forge rendering.
 */
#pragma once

#include <RmlUi/Core/RenderInterface.h>
#include <RmlUi/Core/Types.h>

#include <cstdint>
#include <unordered_map>

struct Renderer;
struct Queue;
struct CmdPool;
struct Cmd;
struct Fence;
struct RenderTarget;
struct Shader;
struct Pipeline;
struct DescriptorSet;
struct Buffer;
struct Sampler;
struct Texture;
struct Descriptor;

class RenderInterface_Forge : public Rml::RenderInterface {
public:
    RenderInterface_Forge();
    ~RenderInterface_Forge() override;

    explicit operator bool() const { return ready; }

    void SetViewport(int width, int height, int offset_x = 0, int offset_y = 0);
    void BeginFrame();
    void EndFrame();

    void Clear() {}
    void SetPresentToBackbuffer(bool) {}
    unsigned int GetOutputTextureId() const { return outputTextureId; }
    int GetOutputWidth() const { return viewport_width; }
    int GetOutputHeight() const { return viewport_height; }

    Rml::CompiledGeometryHandle CompileGeometry(Rml::Span<const Rml::Vertex> vertices,
                                                Rml::Span<const int> indices) override;
    void RenderGeometry(Rml::CompiledGeometryHandle handle,
                        Rml::Vector2f translation,
                        Rml::TextureHandle texture) override;
    void ReleaseGeometry(Rml::CompiledGeometryHandle handle) override;

    Rml::TextureHandle LoadTexture(Rml::Vector2i& texture_dimensions, const Rml::String& source) override;
    Rml::TextureHandle GenerateTexture(Rml::Span<const Rml::byte> source_data,
                                       Rml::Vector2i source_dimensions) override;
    void ReleaseTexture(Rml::TextureHandle texture_handle) override;

    void EnableScissorRegion(bool enable) override { scissor_enabled = enable; }
    void SetScissorRegion(Rml::Rectanglei region) override { scissor_region = region; }

    void EnableClipMask(bool) override {}
    void RenderToClipMask(Rml::ClipMaskOperation, Rml::CompiledGeometryHandle, Rml::Vector2f) override {}

    void SetTransform(const Rml::Matrix4f* transform) override;

    Rml::LayerHandle PushLayer() override { return {}; }
    void CompositeLayers(Rml::LayerHandle, Rml::LayerHandle, Rml::BlendMode,
                         Rml::Span<const Rml::CompiledFilterHandle>) override {}
    void PopLayer() override {}

    Rml::TextureHandle SaveLayerAsTexture() override { return {}; }
    Rml::CompiledFilterHandle SaveLayerAsMaskImage() override { return {}; }
    Rml::CompiledFilterHandle CompileFilter(const Rml::String&, const Rml::Dictionary&) override { return {}; }
    void ReleaseFilter(Rml::CompiledFilterHandle) override {}

    Rml::CompiledShaderHandle CompileShader(const Rml::String&, const Rml::Dictionary&) override { return {}; }
    void RenderShader(Rml::CompiledShaderHandle, Rml::CompiledGeometryHandle, Rml::Vector2f, Rml::TextureHandle) override {}
    void ReleaseShader(Rml::CompiledShaderHandle) override {}

    static constexpr Rml::TextureHandle TextureEnableWithoutBinding = Rml::TextureHandle(-1);
    static constexpr Rml::TextureHandle TexturePostprocess = Rml::TextureHandle(-2);

private:
    struct GeometryData {
        Buffer* vertexBuffer = nullptr;
        Buffer* indexBuffer = nullptr;
        uint32_t indexCount = 0;
    };

    struct TextureData {
        Texture* texture = nullptr;
        int width = 0;
        int height = 0;
        bool external = false;
    };

    bool ensureReady();
    void ensurePipeline();
    void ensureWhiteTexture();
    void ensureRenderTarget(int width, int height);
    const TextureData* lookupTexture(Rml::TextureHandle handle) const;
    void destroyResources();

    bool ready = false;
    int viewport_width = 0;
    int viewport_height = 0;
    int viewport_offset_x = 0;
    int viewport_offset_y = 0;
    bool scissor_enabled = false;
    Rml::Rectanglei scissor_region{};
    Rml::Matrix4f transform;
    Rml::Matrix4f projection;

    Renderer* renderer_ = nullptr;
    Queue* queue_ = nullptr;
    CmdPool* cmdPool_ = nullptr;
    Cmd* cmd_ = nullptr;
    Fence* fence_ = nullptr;
    RenderTarget* uiTarget_ = nullptr;
    uint64_t uiToken_ = 0;
    int uiWidth_ = 0;
    int uiHeight_ = 0;
    uint32_t colorFormat_ = 0;
    bool frameActive_ = false;
    uint32_t debug_draw_calls_ = 0;
    uint32_t debug_triangles_ = 0;
    uint32_t debug_frame_ = 0;

    Shader* shader_ = nullptr;
    Pipeline* pipeline_ = nullptr;
    DescriptorSet* descriptorSet_ = nullptr;
    Buffer* uniformBuffer_ = nullptr;
    Sampler* sampler_ = nullptr;
    Texture* whiteTexture_ = nullptr;
    Descriptor* descriptors_ = nullptr;

    Rml::TextureHandle next_texture_id = 1;
    std::unordered_map<Rml::TextureHandle, TextureData> textures;
    Rml::TextureHandle last_texture = 0;

    unsigned int outputTextureId = 0;
};
