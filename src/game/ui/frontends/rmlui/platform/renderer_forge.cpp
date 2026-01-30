/*
 * RmlUi Forge renderer implementation for BZ3.
 */

#include "ui/frontends/rmlui/platform/renderer_forge.hpp"

#include <RmlUi/Core/Core.h>
#include <RmlUi/Core/FileInterface.h>
#include <RmlUi/Core/Math.h>

#include <stb_image.h>

#include "common/data_path_resolver.hpp"
#include "engine/graphics/backends/forge/ui_bridge.hpp"
#include "spdlog/spdlog.h"

#include "Common_3/Graphics/Interfaces/IGraphics.h"
#include "Common_3/Resources/ResourceLoader/Interfaces/IResourceLoader.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

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
#ifdef None
#undef None
#endif
#ifdef assume
#undef assume
#endif

namespace {

struct UiVertex {
    float x;
    float y;
    float u;
    float v;
    uint32_t color;
};

struct UiConstants {
    float transform[16];
    float translate[4];
};

template <typename ColorT>
uint32_t packColor(const ColorT& color) {
    return static_cast<uint32_t>(color.red)
        | (static_cast<uint32_t>(color.green) << 8)
        | (static_cast<uint32_t>(color.blue) << 16)
        | (static_cast<uint32_t>(color.alpha) << 24);
}

std::vector<uint8_t> readFileBytes(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return {};
    }
    file.seekg(0, std::ios::end);
    const std::streamsize size = file.tellg();
    if (size <= 0) {
        return {};
    }
    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> buffer(static_cast<size_t>(size));
    file.read(reinterpret_cast<char*>(buffer.data()), size);
    return buffer;
}

} // namespace

RenderInterface_Forge::~RenderInterface_Forge() {
    destroyResources();
    if (uiToken_ != 0) {
        graphics_backend::forge_ui::UnregisterExternalTexture(uiToken_);
        uiToken_ = 0;
    }
}

RenderInterface_Forge::RenderInterface_Forge() {
    ready = ensureReady();
    spdlog::warn("RmlUi(Forge): ctor ready={}", ready ? "yes" : "no");
}

void RenderInterface_Forge::SetViewport(int width, int height, int offset_x, int offset_y) {
    viewport_width = Rml::Math::Max(width, 1);
    viewport_height = Rml::Math::Max(height, 1);
    viewport_offset_x = offset_x;
    viewport_offset_y = offset_y;
    projection = Rml::Matrix4f::ProjectOrtho(0, static_cast<float>(viewport_width),
                                             static_cast<float>(viewport_height), 0,
                                             -10000, 10000);
    transform = projection;
    if (ensureReady()) {
        ensureRenderTarget(viewport_width, viewport_height);
    }
    static bool logged = false;
    if (!logged) {
        spdlog::warn("RmlUi(Forge): viewport {}x{} offset {}x{}",
                     viewport_width, viewport_height, viewport_offset_x, viewport_offset_y);
        logged = true;
    }
}

void RenderInterface_Forge::BeginFrame() {
    if (!ensureReady()) {
        static bool logged = false;
        if (!logged) {
            spdlog::warn("RmlUi(Forge): BeginFrame skipped (not ready)");
            logged = true;
        }
        return;
    }
    ensurePipeline();
    ensureWhiteTexture();
    ensureRenderTarget(viewport_width, viewport_height);
    if (!uiTarget_ || !pipeline_ || !descriptorSet_ || !uniformBuffer_ || !sampler_) {
        static bool logged = false;
        if (!logged) {
            spdlog::warn("RmlUi(Forge): BeginFrame skipped (uiTarget={}, pipeline={}, set={}, ub={}, sampler={})",
                         uiTarget_ ? "yes" : "no",
                         pipeline_ ? "yes" : "no",
                         descriptorSet_ ? "yes" : "no",
                         uniformBuffer_ ? "yes" : "no",
                         sampler_ ? "yes" : "no");
            logged = true;
        }
        return;
    }
    static bool loggedBegin = false;
    if (!loggedBegin) {
        spdlog::warn("RmlUi(Forge): begin frame target {}x{}",
                     uiTarget_ ? static_cast<int>(uiTarget_->mWidth) : -1,
                     uiTarget_ ? static_cast<int>(uiTarget_->mHeight) : -1);
        loggedBegin = true;
    }

    if (cmdPool_) {
        resetCmdPool(renderer_, cmdPool_);
    }
    if (!cmd_) {
        return;
    }

    beginCmd(cmd_);

    RenderTargetBarrier rtBegin{};
    rtBegin.pRenderTarget = uiTarget_;
    rtBegin.mCurrentState = RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    rtBegin.mNewState = RESOURCE_STATE_RENDER_TARGET;
    cmdResourceBarrier(cmd_, 0, nullptr, 0, nullptr, 1, &rtBegin);

    BindRenderTargetsDesc bindDesc{};
    bindDesc.mRenderTargetCount = 1;
    bindDesc.mRenderTargets[0].pRenderTarget = uiTarget_;
    bindDesc.mRenderTargets[0].mLoadAction = LOAD_ACTION_CLEAR;
    bindDesc.mRenderTargets[0].mStoreAction = STORE_ACTION_STORE;
    const bool debugClear = std::getenv("BZ3_RMLUI_DEBUG_CLEAR") != nullptr;
    if (debugClear) {
        bindDesc.mRenderTargets[0].mClearValue = {1.0f, 0.0f, 1.0f, 1.0f};
    } else {
        bindDesc.mRenderTargets[0].mClearValue = {0.0f, 0.0f, 0.0f, 0.0f};
    }
    bindDesc.mRenderTargets[0].mOverrideClearValue = 1;
    bindDesc.mDepthStencil.pDepthStencil = nullptr;
    bindDesc.mDepthStencil.mLoadAction = LOAD_ACTION_DONTCARE;
    bindDesc.mDepthStencil.mStoreAction = STORE_ACTION_DONTCARE;
    cmdBindRenderTargets(cmd_, &bindDesc);

    cmdSetViewport(cmd_, 0.0f, 0.0f, static_cast<float>(viewport_width),
                   static_cast<float>(viewport_height), 0.0f, 1.0f);
    cmdSetScissor(cmd_, 0, 0, static_cast<uint32_t>(viewport_width),
                  static_cast<uint32_t>(viewport_height));
    frameActive_ = true;
    debug_draw_calls_ = 0;
    debug_triangles_ = 0;
    debug_frame_++;

    if (std::getenv("BZ3_RMLUI_DEBUG_TRIANGLE")) {
        ensureDebugTriangleBuffers();
        if (debugTriangleVB_ && debugTriangleIB_ && uniformBuffer_ && sampler_ && whiteTexture_) {
            UiVertex tri[3] = {
                {-0.5f, -0.5f, 0.0f, 0.0f, 0xffffffffu},
                { 0.0f,  0.5f, 0.0f, 0.0f, 0xffffffffu},
                { 0.5f, -0.5f, 0.0f, 0.0f, 0xffffffffu},
            };
            uint16_t idx[3] = {0, 1, 2};

            BufferUpdateDesc vbUpdate = { debugTriangleVB_ };
            beginUpdateResource(&vbUpdate);
            std::memcpy(vbUpdate.pMappedData, tri, sizeof(tri));
            endUpdateResource(&vbUpdate);

            BufferUpdateDesc ibUpdate = { debugTriangleIB_ };
            beginUpdateResource(&ibUpdate);
            std::memcpy(ibUpdate.pMappedData, idx, sizeof(idx));
            endUpdateResource(&ibUpdate);

            UiConstants constants{};
            std::memset(constants.transform, 0, sizeof(constants.transform));
            constants.transform[0] = 1.0f;
            constants.transform[5] = 1.0f;
            constants.transform[10] = 1.0f;
            constants.transform[15] = 1.0f;
            constants.translate[0] = 0.0f;
            constants.translate[1] = 0.0f;
            constants.translate[2] = 0.0f;
            constants.translate[3] = 0.0f;
            BufferUpdateDesc cbUpdate = { uniformBuffer_ };
            beginUpdateResource(&cbUpdate);
            std::memcpy(cbUpdate.pMappedData, &constants, sizeof(constants));
            endUpdateResource(&cbUpdate);

            Texture* drawTexture = whiteTexture_;
            DescriptorData params[3] = {};
            params[0].mIndex = 0;
            params[0].ppBuffers = &uniformBuffer_;
            params[1].mIndex = 1;
            params[1].ppTextures = &drawTexture;
            params[2].mIndex = 2;
            params[2].ppSamplers = &sampler_;
            updateDescriptorSet(renderer_, 0, descriptorSet_, 3, params);

            cmdBindPipeline(cmd_, pipeline_);
            cmdBindDescriptorSet(cmd_, 0, descriptorSet_);
            uint32_t stride = sizeof(UiVertex);
            uint64_t offset = 0;
            cmdBindVertexBuffer(cmd_, 0, &debugTriangleVB_, &stride, &offset);
            cmdBindIndexBuffer(cmd_, debugTriangleIB_, INDEX_TYPE_UINT16, 0);
            cmdDrawIndexed(cmd_, 3, 0, 0);
        }
    }

    (void)viewport_offset_x;
    (void)viewport_offset_y;
}

void RenderInterface_Forge::EndFrame() {
    if (!frameActive_ || !cmd_ || !uiTarget_ || !queue_) {
        return;
    }

    RenderTargetBarrier rtEnd{};
    rtEnd.pRenderTarget = uiTarget_;
    rtEnd.mCurrentState = RESOURCE_STATE_RENDER_TARGET;
    rtEnd.mNewState = RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    cmdResourceBarrier(cmd_, 0, nullptr, 0, nullptr, 1, &rtEnd);

    endCmd(cmd_);

    QueueSubmitDesc submitDesc{};
    submitDesc.ppCmds = &cmd_;
    submitDesc.mCmdCount = 1;
    submitDesc.pSignalFence = fence_;
    queueSubmit(queue_, &submitDesc);
    if (fence_) {
        waitForFences(renderer_, 1, &fence_);
    }
    frameActive_ = false;
    if (debug_frame_ % 120 == 0) {
        spdlog::warn("RmlUi(Forge): frame {} draw_calls={} tris={}",
                     debug_frame_, debug_draw_calls_, debug_triangles_);
    }
}

Rml::CompiledGeometryHandle RenderInterface_Forge::CompileGeometry(Rml::Span<const Rml::Vertex> vertices,
                                                                    Rml::Span<const int> indices) {
    if (!ensureReady() || vertices.empty() || indices.empty()) {
        static bool logged = false;
        if (!logged) {
            spdlog::warn("RmlUi(Forge): CompileGeometry skipped (ready={} vtx={} idx={})",
                         ensureReady() ? "yes" : "no",
                         vertices.size(),
                         indices.size());
            logged = true;
        }
        return {};
    }

    std::vector<UiVertex> packed_vertices;
    packed_vertices.resize(vertices.size());
    for (size_t i = 0; i < vertices.size(); ++i) {
        packed_vertices[i].x = vertices[i].position.x;
        packed_vertices[i].y = vertices[i].position.y;
        packed_vertices[i].u = vertices[i].tex_coord.x;
        packed_vertices[i].v = vertices[i].tex_coord.y;
        packed_vertices[i].color = packColor(vertices[i].colour);
    }

    std::vector<uint32_t> packed_indices;
    packed_indices.resize(indices.size());
    for (size_t i = 0; i < indices.size(); ++i) {
        packed_indices[i] = static_cast<uint32_t>(indices[i]);
    }

    auto* geometry = new GeometryData();
    geometry->indexCount = static_cast<uint32_t>(packed_indices.size());

    BufferLoadDesc vbDesc{};
    vbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
    vbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
    vbDesc.mDesc.mSize = packed_vertices.size() * sizeof(UiVertex);
    vbDesc.mDesc.pName = "RmlUi Forge VB";
    vbDesc.pData = packed_vertices.data();
    vbDesc.ppBuffer = &geometry->vertexBuffer;
    SyncToken vbToken{};
    addResource(&vbDesc, &vbToken);
    waitForToken(&vbToken);

    BufferLoadDesc ibDesc{};
    ibDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_INDEX_BUFFER;
    ibDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
    ibDesc.mDesc.mSize = packed_indices.size() * sizeof(uint32_t);
    ibDesc.mDesc.pName = "RmlUi Forge IB";
    ibDesc.pData = packed_indices.data();
    ibDesc.ppBuffer = &geometry->indexBuffer;
    SyncToken ibToken{};
    addResource(&ibDesc, &ibToken);
    waitForToken(&ibToken);

    if (!geometry->vertexBuffer || !geometry->indexBuffer) {
        ReleaseGeometry(reinterpret_cast<Rml::CompiledGeometryHandle>(geometry));
        return {};
    }

    return reinterpret_cast<Rml::CompiledGeometryHandle>(geometry);
}

void RenderInterface_Forge::RenderGeometry(Rml::CompiledGeometryHandle handle,
                                           Rml::Vector2f translation,
                                           Rml::TextureHandle texture) {
    if (!frameActive_ || !handle || !cmd_ || !renderer_ || !pipeline_) {
        static bool logged = false;
        if (!logged) {
            spdlog::warn("RmlUi(Forge): RenderGeometry skipped (frameActive={}, handle={}, cmd={}, renderer={}, pipeline={})",
                         frameActive_ ? "yes" : "no",
                         handle ? "yes" : "no",
                         cmd_ ? "yes" : "no",
                         renderer_ ? "yes" : "no",
                         pipeline_ ? "yes" : "no");
            logged = true;
        }
        return;
    }
    if (texture == TexturePostprocess) {
        return;
    }

    GeometryData* geometry = reinterpret_cast<GeometryData*>(handle);
    if (!geometry->vertexBuffer || !geometry->indexBuffer || geometry->indexCount == 0) {
        return;
    }

    bool use_texture = (texture != 0);
    const TextureData* tex_data = nullptr;
    if (use_texture && texture != TextureEnableWithoutBinding) {
        tex_data = lookupTexture(texture);
        if (!tex_data || !tex_data->texture) {
            use_texture = false;
        } else {
            last_texture = texture;
        }
    } else if (use_texture && texture == TextureEnableWithoutBinding) {
        tex_data = lookupTexture(last_texture);
        if (!tex_data || !tex_data->texture) {
            use_texture = false;
        }
    }

    Texture* drawTexture = whiteTexture_;
    if (use_texture && tex_data && tex_data->texture) {
        drawTexture = tex_data->texture;
    }
    if (!drawTexture) {
        return;
    }
    static bool loggedDraw = false;
    if (!loggedDraw) {
        spdlog::warn("RmlUi(Forge): draw translation {} {} scissor={} region {} {} {} {}",
                     translation.x, translation.y,
                     scissor_enabled ? "on" : "off",
                     scissor_region.p0.x, scissor_region.p0.y,
                     scissor_region.p1.x, scissor_region.p1.y);
        spdlog::warn("RmlUi(Forge): transform [{} {} {} {}] [{} {} {} {}] [{} {} {} {}] [{} {} {} {}]",
                     transform.data()[0], transform.data()[1], transform.data()[2], transform.data()[3],
                     transform.data()[4], transform.data()[5], transform.data()[6], transform.data()[7],
                     transform.data()[8], transform.data()[9], transform.data()[10], transform.data()[11],
                     transform.data()[12], transform.data()[13], transform.data()[14], transform.data()[15]);
        loggedDraw = true;
    }

    if (scissor_enabled && scissor_region.Valid()) {
        const int x = std::max(0, scissor_region.p0.x);
        const int y = std::max(0, scissor_region.p0.y);
        const int w = std::max(0, scissor_region.Width());
        const int h = std::max(0, scissor_region.Height());
        cmdSetScissor(cmd_, static_cast<uint32_t>(x), static_cast<uint32_t>(y),
                      static_cast<uint32_t>(w), static_cast<uint32_t>(h));
    } else {
        cmdSetScissor(cmd_, 0, 0, static_cast<uint32_t>(viewport_width),
                      static_cast<uint32_t>(viewport_height));
    }

    UiConstants constants{};
    std::memcpy(constants.transform, transform.data(), sizeof(constants.transform));
    constants.translate[0] = translation.x;
    constants.translate[1] = translation.y;
    constants.translate[2] = 0.0f;
    constants.translate[3] = 0.0f;

    BufferUpdateDesc cbUpdate = { uniformBuffer_ };
    beginUpdateResource(&cbUpdate);
    std::memcpy(cbUpdate.pMappedData, &constants, sizeof(constants));
    endUpdateResource(&cbUpdate);

    DescriptorData params[3] = {};
    params[0].mIndex = 0;
    params[0].ppBuffers = &uniformBuffer_;
    params[1].mIndex = 1;
    params[1].ppTextures = &drawTexture;
    params[2].mIndex = 2;
    params[2].ppSamplers = &sampler_;
    updateDescriptorSet(renderer_, 0, descriptorSet_, 3, params);

    cmdBindPipeline(cmd_, pipeline_);
    cmdBindDescriptorSet(cmd_, 0, descriptorSet_);

    uint32_t stride = sizeof(UiVertex);
    uint64_t offset = 0;
    cmdBindVertexBuffer(cmd_, 0, &geometry->vertexBuffer, &stride, &offset);
    cmdBindIndexBuffer(cmd_, geometry->indexBuffer, INDEX_TYPE_UINT32, 0);
    cmdDrawIndexed(cmd_, geometry->indexCount, 0, 0);
    debug_draw_calls_++;
    debug_triangles_ += geometry->indexCount / 3;
}

void RenderInterface_Forge::ReleaseGeometry(Rml::CompiledGeometryHandle handle) {
    if (!handle) {
        return;
    }
    auto* geometry = reinterpret_cast<GeometryData*>(handle);
    if (renderer_) {
        if (geometry->vertexBuffer) {
            removeResource(geometry->vertexBuffer);
        }
        if (geometry->indexBuffer) {
            removeResource(geometry->indexBuffer);
        }
    }
    delete geometry;
}

Rml::TextureHandle RenderInterface_Forge::LoadTexture(Rml::Vector2i& texture_dimensions, const Rml::String& source) {
    const Rml::String tex_prefix = "texid:";
    if (source.rfind(tex_prefix, 0) == 0) {
        const char* id_str = source.c_str() + tex_prefix.size();
        char* end_ptr = nullptr;
        const unsigned long token = std::strtoul(id_str, &end_ptr, 10);
        if (token == 0) {
            return {};
        }
        int width = 1;
        int height = 1;
        if (end_ptr && *end_ptr == ':') {
            int parsed_w = 0;
            int parsed_h = 0;
            if (std::sscanf(end_ptr + 1, "%dx%d", &parsed_w, &parsed_h) == 2) {
                if (parsed_w > 0 && parsed_h > 0) {
                    width = parsed_w;
                    height = parsed_h;
                }
            }
        }
        TextureData entry;
        entry.external = true;
        entry.width = width;
        entry.height = height;
        entry.texture = static_cast<Texture*>(graphics_backend::forge_ui::ResolveExternalTexture(token));
        if (!entry.texture) {
            return {};
        }
        const Rml::TextureHandle handle = next_texture_id++;
        textures.emplace(handle, entry);
        texture_dimensions.x = entry.width;
        texture_dimensions.y = entry.height;
        return handle;
    }

    Rml::FileInterface* file_interface = Rml::GetFileInterface();
    if (!file_interface) {
        return {};
    }
    Rml::FileHandle handle = file_interface->Open(source);
    if (!handle) {
        return {};
    }
    const size_t size = file_interface->Length(handle);
    if (size == 0) {
        file_interface->Close(handle);
        return {};
    }
    std::vector<uint8_t> buffer(size);
    file_interface->Read(buffer.data(), size, handle);
    file_interface->Close(handle);

    int width = 0;
    int height = 0;
    int channels = 0;
    unsigned char* data = stbi_load_from_memory(buffer.data(), static_cast<int>(buffer.size()), &width, &height, &channels, 4);
    if (!data) {
        return {};
    }

    Rml::Span<const Rml::byte> span(reinterpret_cast<const Rml::byte*>(data),
                                    static_cast<size_t>(width * height * 4));
    Rml::TextureHandle texture_handle = GenerateTexture(span, Rml::Vector2i{width, height});
    stbi_image_free(data);
    if (texture_handle) {
        texture_dimensions.x = width;
        texture_dimensions.y = height;
    }
    return texture_handle;
}

Rml::TextureHandle RenderInterface_Forge::GenerateTexture(Rml::Span<const Rml::byte> source_data,
                                                          Rml::Vector2i source_dimensions) {
    if (!ensureReady() || source_dimensions.x <= 0 || source_dimensions.y <= 0 || source_data.empty()) {
        return {};
    }

    TextureDesc textureDesc{};
    textureDesc.mArraySize = 1;
    textureDesc.mDepth = 1;
    textureDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
    textureDesc.mFormat = TinyImageFormat_R8G8B8A8_UNORM;
    textureDesc.mHeight = static_cast<uint32_t>(source_dimensions.y);
    textureDesc.mMipLevels = 1;
    textureDesc.mSampleCount = SAMPLE_COUNT_1;
    textureDesc.mStartState = RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    textureDesc.mWidth = static_cast<uint32_t>(source_dimensions.x);
    textureDesc.pName = "RmlUi Forge Texture";

    Texture* texture = nullptr;
    TextureLoadDesc loadDesc{};
    loadDesc.pDesc = &textureDesc;
    loadDesc.ppTexture = &texture;
    SyncToken token{};
    addResource(&loadDesc, &token);
    waitForToken(&token);

    if (!texture) {
        return {};
    }

    TextureUpdateDesc updateDesc = { texture, 0, 1, 0, 1, RESOURCE_STATE_PIXEL_SHADER_RESOURCE };
    beginUpdateResource(&updateDesc);
    TextureSubresourceUpdate subresource = updateDesc.getSubresourceUpdateDesc(0, 0);
    const uint8_t* src = reinterpret_cast<const uint8_t*>(source_data.data());
    const uint32_t rowBytes = static_cast<uint32_t>(source_dimensions.x * 4);
    for (uint32_t row = 0; row < subresource.mRowCount; ++row) {
        std::memcpy(subresource.pMappedData + row * subresource.mDstRowStride,
                    src + row * rowBytes,
                    rowBytes);
    }
    endUpdateResource(&updateDesc);
    if (fence_) {
        FlushResourceUpdateDesc flush{};
        flush.pOutFence = fence_;
        flushResourceUpdates(&flush);
        waitForFences(renderer_, 1, &fence_);
    }

    TextureData entry{};
    entry.texture = texture;
    entry.width = source_dimensions.x;
    entry.height = source_dimensions.y;
    entry.external = false;
    const Rml::TextureHandle handle = next_texture_id++;
    textures.emplace(handle, entry);
    return handle;
}

void RenderInterface_Forge::ReleaseTexture(Rml::TextureHandle texture_handle) {
    if (texture_handle == 0) {
        return;
    }
    auto it = textures.find(texture_handle);
    if (it == textures.end()) {
        return;
    }
    if (renderer_ && it->second.texture && !it->second.external) {
        removeResource(it->second.texture);
    }
    textures.erase(it);
}

void RenderInterface_Forge::SetTransform(const Rml::Matrix4f* transformIn) {
    if (transformIn) {
        transform = projection * (*transformIn);
    } else {
        transform = projection;
    }
}

bool RenderInterface_Forge::ensureReady() {
    if (ready) {
        return true;
    }
    auto ctx = graphics_backend::forge_ui::GetContext();
    if (!ctx.renderer || !ctx.graphicsQueue) {
        return false;
    }
    renderer_ = static_cast<Renderer*>(ctx.renderer);
    queue_ = static_cast<Queue*>(ctx.graphicsQueue);
    colorFormat_ = ctx.colorFormat;

    if (!cmdPool_) {
        CmdPoolDesc poolDesc{};
        poolDesc.pQueue = queue_;
        initCmdPool(renderer_, &poolDesc, &cmdPool_);
    }
    if (!cmd_ && cmdPool_) {
        CmdDesc cmdDesc{};
        cmdDesc.pPool = cmdPool_;
        initCmd(renderer_, &cmdDesc, &cmd_);
    }
    if (!fence_) {
        initFence(renderer_, &fence_);
    }

    ensurePipeline();
    ready = renderer_ != nullptr && pipeline_ != nullptr;
    return ready;
}

void RenderInterface_Forge::ensurePipeline() {
    if (!renderer_ || pipeline_) {
        return;
    }

    const std::filesystem::path shaderDir = bz::data::Resolve("forge/shaders");
    const auto vsPath = shaderDir / "rmlui.vert.spv";
    const auto fsPath = shaderDir / "rmlui.frag.spv";
    auto vsBytes = readFileBytes(vsPath);
    auto fsBytes = readFileBytes(fsPath);
    if (vsBytes.empty() || fsBytes.empty()) {
        spdlog::error("RmlUi(Forge): missing shaders '{}', '{}'", vsPath.string(), fsPath.string());
        return;
    }
    spdlog::warn("RmlUi(Forge): loaded shaders '{}' ({} bytes), '{}' ({} bytes)",
                 vsPath.string(), vsBytes.size(), fsPath.string(), fsBytes.size());

    BinaryShaderDesc shaderDesc{};
    shaderDesc.mStages = SHADER_STAGE_VERT | SHADER_STAGE_FRAG;
    shaderDesc.mOwnByteCode = false;
    shaderDesc.mVert = { "rmlui.vert", vsBytes.data(), static_cast<uint32_t>(vsBytes.size()), "main" };
    shaderDesc.mFrag = { "rmlui.frag", fsBytes.data(), static_cast<uint32_t>(fsBytes.size()), "main" };
    addShaderBinary(renderer_, &shaderDesc, &shader_);
    if (!shader_) {
        spdlog::error("RmlUi(Forge): failed to create shader");
        return;
    }

    SamplerDesc samplerDesc{};
    samplerDesc.mMinFilter = FILTER_LINEAR;
    samplerDesc.mMagFilter = FILTER_LINEAR;
    samplerDesc.mMipMapMode = MIPMAP_MODE_LINEAR;
    samplerDesc.mAddressU = ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerDesc.mAddressV = ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerDesc.mAddressW = ADDRESS_MODE_CLAMP_TO_EDGE;
    addSampler(renderer_, &samplerDesc, &sampler_);

    if (!descriptors_) {
        descriptors_ = new Descriptor[3]{};
    }
    descriptors_[0].mType = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptors_[0].mCount = 1;
    descriptors_[0].mOffset = 0;
    descriptors_[1].mType = DESCRIPTOR_TYPE_TEXTURE;
    descriptors_[1].mCount = 1;
    descriptors_[1].mOffset = 1;
    descriptors_[2].mType = DESCRIPTOR_TYPE_SAMPLER;
    descriptors_[2].mCount = 1;
    descriptors_[2].mOffset = 2;

    DescriptorSetDesc setDesc{};
    setDesc.mIndex = 0;
    setDesc.mMaxSets = 1;
    setDesc.mDescriptorCount = 3;
    setDesc.pDescriptors = descriptors_;
    addDescriptorSet(renderer_, &setDesc, &descriptorSet_);

    BufferLoadDesc ubDesc{};
    ubDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    ubDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
    ubDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
    ubDesc.mDesc.mStartState = RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
    ubDesc.mDesc.mSize = sizeof(UiConstants);
    ubDesc.mDesc.pName = "RmlUi Forge Uniform";
    ubDesc.ppBuffer = &uniformBuffer_;
    addResource(&ubDesc, nullptr);

    VertexLayout layout{};
    layout.mBindingCount = 1;
    layout.mAttribCount = 3;
    layout.mBindings[0].mStride = sizeof(UiVertex);
    layout.mBindings[0].mRate = VERTEX_BINDING_RATE_VERTEX;
    layout.mAttribs[0].mSemantic = SEMANTIC_POSITION;
    layout.mAttribs[0].mFormat = TinyImageFormat_R32G32_SFLOAT;
    layout.mAttribs[0].mBinding = 0;
    layout.mAttribs[0].mLocation = 0;
    layout.mAttribs[0].mOffset = 0;
    layout.mAttribs[1].mSemantic = SEMANTIC_TEXCOORD0;
    layout.mAttribs[1].mFormat = TinyImageFormat_R32G32_SFLOAT;
    layout.mAttribs[1].mBinding = 0;
    layout.mAttribs[1].mLocation = 1;
    layout.mAttribs[1].mOffset = sizeof(float) * 2;
    layout.mAttribs[2].mSemantic = SEMANTIC_COLOR;
    layout.mAttribs[2].mFormat = TinyImageFormat_R8G8B8A8_UNORM;
    layout.mAttribs[2].mBinding = 0;
    layout.mAttribs[2].mLocation = 2;
    layout.mAttribs[2].mOffset = sizeof(float) * 4;

    BlendStateDesc blend{};
    blend.mSrcFactors[0] = BC_ONE;
    blend.mDstFactors[0] = BC_ONE_MINUS_SRC_ALPHA;
    blend.mSrcAlphaFactors[0] = BC_ONE;
    blend.mDstAlphaFactors[0] = BC_ONE_MINUS_SRC_ALPHA;
    blend.mColorWriteMasks[0] = COLOR_MASK_ALL;
    blend.mRenderTargetMask = BLEND_STATE_TARGET_ALL;
    blend.mIndependentBlend = false;

    DepthStateDesc depth{};
    depth.mDepthTest = false;
    depth.mDepthWrite = false;

    RasterizerStateDesc raster{};
    raster.mCullMode = CULL_MODE_NONE;
    raster.mScissor = true;

    TinyImageFormat colorFormat = static_cast<TinyImageFormat>(colorFormat_ != 0 ? colorFormat_ : TinyImageFormat_R8G8B8A8_UNORM);
    PipelineDesc pipelineDesc{};
    pipelineDesc.mType = PIPELINE_TYPE_GRAPHICS;
    pipelineDesc.mGraphicsDesc.pShaderProgram = shader_;
    pipelineDesc.mGraphicsDesc.pVertexLayout = &layout;
    pipelineDesc.mGraphicsDesc.pBlendState = &blend;
    pipelineDesc.mGraphicsDesc.pDepthState = &depth;
    pipelineDesc.mGraphicsDesc.pRasterizerState = &raster;
    pipelineDesc.mGraphicsDesc.mRenderTargetCount = 1;
    pipelineDesc.mGraphicsDesc.mSampleCount = SAMPLE_COUNT_1;
    pipelineDesc.mGraphicsDesc.mSampleQuality = 0;
    pipelineDesc.mGraphicsDesc.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
    pipelineDesc.mGraphicsDesc.pColorFormats = &colorFormat;
    pipelineDesc.mGraphicsDesc.mDepthStencilFormat = TinyImageFormat_UNDEFINED;

    DescriptorSetLayoutDesc layoutDesc{};
    layoutDesc.pDescriptors = descriptors_;
    layoutDesc.mDescriptorCount = 3;
    layoutDesc.pStaticSamplers = nullptr;
    layoutDesc.mStaticSamplerCount = 0;
    const DescriptorSetLayoutDesc* layoutPtrs[] = { &layoutDesc };
    pipelineDesc.pLayouts = layoutPtrs;
    pipelineDesc.mLayoutCount = 1;

    addPipeline(renderer_, &pipelineDesc, &pipeline_);
    if (!pipeline_) {
        spdlog::error("RmlUi(Forge): failed to create pipeline");
    }
}

void RenderInterface_Forge::ensureWhiteTexture() {
    if (whiteTexture_ || !renderer_) {
        return;
    }
    const uint32_t white = 0xffffffffu;
    TextureDesc textureDesc{};
    textureDesc.mArraySize = 1;
    textureDesc.mDepth = 1;
    textureDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
    textureDesc.mFormat = TinyImageFormat_R8G8B8A8_UNORM;
    textureDesc.mHeight = 1;
    textureDesc.mMipLevels = 1;
    textureDesc.mSampleCount = SAMPLE_COUNT_1;
    textureDesc.mStartState = RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    textureDesc.mWidth = 1;
    textureDesc.pName = "RmlUi Forge White Texture";

    TextureLoadDesc loadDesc{};
    loadDesc.pDesc = &textureDesc;
    loadDesc.ppTexture = &whiteTexture_;
    SyncToken token{};
    addResource(&loadDesc, &token);
    waitForToken(&token);
    if (!whiteTexture_) {
        return;
    }

    TextureUpdateDesc updateDesc = { whiteTexture_, 0, 1, 0, 1, RESOURCE_STATE_PIXEL_SHADER_RESOURCE };
    beginUpdateResource(&updateDesc);
    TextureSubresourceUpdate subresource = updateDesc.getSubresourceUpdateDesc(0, 0);
    std::memcpy(subresource.pMappedData, &white, sizeof(white));
    endUpdateResource(&updateDesc);
    if (fence_) {
        FlushResourceUpdateDesc flush{};
        flush.pOutFence = fence_;
        flushResourceUpdates(&flush);
        waitForFences(renderer_, 1, &fence_);
    }
}

void RenderInterface_Forge::ensureRenderTarget(int width, int height) {
    if (!renderer_) {
        return;
    }
    if (width <= 0 || height <= 0) {
        if (uiToken_ != 0) {
            graphics_backend::forge_ui::UnregisterExternalTexture(uiToken_);
            uiToken_ = 0;
        }
        if (uiTarget_) {
            removeRenderTarget(renderer_, uiTarget_);
            uiTarget_ = nullptr;
        }
        outputTextureId = 0;
        uiWidth_ = 0;
        uiHeight_ = 0;
        return;
    }
    if (uiTarget_ && width == uiWidth_ && height == uiHeight_) {
        return;
    }
    if (uiToken_ != 0) {
        graphics_backend::forge_ui::UnregisterExternalTexture(uiToken_);
        uiToken_ = 0;
    }
    if (uiTarget_) {
        removeRenderTarget(renderer_, uiTarget_);
        uiTarget_ = nullptr;
    }

    RenderTargetDesc rtDesc{};
    rtDesc.mWidth = static_cast<uint32_t>(width);
    rtDesc.mHeight = static_cast<uint32_t>(height);
    rtDesc.mDepth = 1;
    rtDesc.mArraySize = 1;
    rtDesc.mMipLevels = 1;
    rtDesc.mSampleCount = SAMPLE_COUNT_1;
    rtDesc.mSampleQuality = 0;
    rtDesc.mFormat = static_cast<TinyImageFormat>(colorFormat_ != 0 ? colorFormat_ : TinyImageFormat_R8G8B8A8_UNORM);
    rtDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
    rtDesc.mStartState = RESOURCE_STATE_RENDER_TARGET;
    rtDesc.pName = "RmlUi Forge UI RT";
    addRenderTarget(renderer_, &rtDesc, &uiTarget_);

    if (uiTarget_ && uiTarget_->pTexture) {
        uiToken_ = graphics_backend::forge_ui::RegisterExternalTexture(uiTarget_->pTexture);
        outputTextureId = static_cast<unsigned int>(uiToken_);
        spdlog::warn("RmlUi(Forge): output texture token={} size={}x{}", outputTextureId, width, height);
    }
    uiWidth_ = width;
    uiHeight_ = height;
}

const RenderInterface_Forge::TextureData* RenderInterface_Forge::lookupTexture(Rml::TextureHandle handle) const {
    auto it = textures.find(handle);
    if (it == textures.end()) {
        return nullptr;
    }
    return &it->second;
}

void RenderInterface_Forge::destroyResources() {
    if (renderer_) {
        for (auto& entry : textures) {
            if (entry.second.texture && !entry.second.external) {
                removeResource(entry.second.texture);
            }
        }
        textures.clear();
        if (whiteTexture_) {
            removeResource(whiteTexture_);
        }
        if (pipeline_) {
            removePipeline(renderer_, pipeline_);
        }
        if (shader_) {
            removeShader(renderer_, shader_);
        }
        if (descriptorSet_) {
            removeDescriptorSet(renderer_, descriptorSet_);
        }
        delete[] descriptors_;
        descriptors_ = nullptr;
        if (sampler_) {
            removeSampler(renderer_, sampler_);
        }
        if (uniformBuffer_) {
            removeResource(uniformBuffer_);
        }
        if (debugTriangleVB_) {
            removeResource(debugTriangleVB_);
        }
        if (debugTriangleIB_) {
            removeResource(debugTriangleIB_);
        }
        if (uiTarget_) {
            removeRenderTarget(renderer_, uiTarget_);
        }
        if (cmd_) {
            exitCmd(renderer_, cmd_);
        }
        if (cmdPool_) {
            exitCmdPool(renderer_, cmdPool_);
        }
        if (fence_) {
            exitFence(renderer_, fence_);
        }
    }
    pipeline_ = nullptr;
    shader_ = nullptr;
    descriptorSet_ = nullptr;
    sampler_ = nullptr;
    uniformBuffer_ = nullptr;
    debugTriangleVB_ = nullptr;
    debugTriangleIB_ = nullptr;
    whiteTexture_ = nullptr;
    uiTarget_ = nullptr;
    cmd_ = nullptr;
    cmdPool_ = nullptr;
    fence_ = nullptr;
    ready = false;
    uiWidth_ = 0;
    uiHeight_ = 0;
}

void RenderInterface_Forge::ensureDebugTriangleBuffers() {
    if (!renderer_) {
        return;
    }
    if (debugTriangleVB_ && debugTriangleIB_) {
        return;
    }

    BufferLoadDesc vbDesc{};
    vbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
    vbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
    vbDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
    vbDesc.mDesc.mSize = sizeof(UiVertex) * 3;
    vbDesc.mDesc.pName = "RmlUi Forge Debug VB";
    vbDesc.ppBuffer = &debugTriangleVB_;
    addResource(&vbDesc, nullptr);

    BufferLoadDesc ibDesc{};
    ibDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_INDEX_BUFFER;
    ibDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
    ibDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
    ibDesc.mDesc.mSize = sizeof(uint16_t) * 3;
    ibDesc.mDesc.pName = "RmlUi Forge Debug IB";
    ibDesc.ppBuffer = &debugTriangleIB_;
    addResource(&ibDesc, nullptr);
}
