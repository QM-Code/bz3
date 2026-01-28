/*
 * RmlUi Forge renderer stub for BZ3.
 * TODO: Implement actual Forge rendering.
 */

#include "ui/frontends/rmlui/platform/renderer_forge.hpp"

#include <RmlUi/Core/Math.h>

void RenderInterface_Forge::SetViewport(int width, int height, int offset_x, int offset_y) {
    viewport_width = Rml::Math::Max(width, 1);
    viewport_height = Rml::Math::Max(height, 1);
    viewport_offset_x = offset_x;
    viewport_offset_y = offset_y;
    projection = Rml::Matrix4f::ProjectOrtho(0, static_cast<float>(viewport_width),
                                             static_cast<float>(viewport_height), 0,
                                             -10000, 10000);
    transform = projection;
}

void RenderInterface_Forge::BeginFrame() {
}

void RenderInterface_Forge::EndFrame() {
}

Rml::CompiledGeometryHandle RenderInterface_Forge::CompileGeometry(Rml::Span<const Rml::Vertex> vertices,
                                                                    Rml::Span<const int> indices) {
    (void)vertices;
    (void)indices;
    return {};
}

void RenderInterface_Forge::RenderGeometry(Rml::CompiledGeometryHandle handle,
                                           Rml::Vector2f translation,
                                           Rml::TextureHandle texture) {
    (void)handle;
    (void)translation;
    (void)texture;
}

void RenderInterface_Forge::ReleaseGeometry(Rml::CompiledGeometryHandle handle) {
    (void)handle;
}

Rml::TextureHandle RenderInterface_Forge::LoadTexture(Rml::Vector2i& texture_dimensions, const Rml::String& source) {
    (void)texture_dimensions;
    (void)source;
    return {};
}

Rml::TextureHandle RenderInterface_Forge::GenerateTexture(Rml::Span<const Rml::byte> source_data,
                                                          Rml::Vector2i source_dimensions) {
    (void)source_data;
    (void)source_dimensions;
    return {};
}

void RenderInterface_Forge::ReleaseTexture(Rml::TextureHandle texture_handle) {
    (void)texture_handle;
}

void RenderInterface_Forge::SetTransform(const Rml::Matrix4f* transformIn) {
    if (transformIn) {
        transform = projection * (*transformIn);
    } else {
        transform = projection;
    }
}
