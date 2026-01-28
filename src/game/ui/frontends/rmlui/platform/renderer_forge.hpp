/*
 * RmlUi Forge renderer stub for BZ3.
 * TODO: Implement actual Forge rendering.
 */
#pragma once

#include <RmlUi/Core/RenderInterface.h>
#include <RmlUi/Core/Types.h>

#include <cstdint>

class RenderInterface_Forge : public Rml::RenderInterface {
public:
    RenderInterface_Forge() = default;
    ~RenderInterface_Forge() override = default;

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
    bool ready = true;
    int viewport_width = 0;
    int viewport_height = 0;
    int viewport_offset_x = 0;
    int viewport_offset_y = 0;
    bool scissor_enabled = false;
    Rml::Rectanglei scissor_region{};
    Rml::Matrix4f transform;
    Rml::Matrix4f projection;
    unsigned int outputTextureId = 0;
};
