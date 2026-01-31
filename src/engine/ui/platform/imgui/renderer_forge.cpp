#include "karma/ui/platform/imgui/renderer_forge.hpp"

#include <imgui.h>

#include "karma/graphics/backends/forge/ui_bridge.hpp"
#include "Common_3/Resources/ResourceLoader/Interfaces/IResourceLoader.h"
#include "karma/ui/imgui/texture_utils.hpp"

#ifdef assume
#undef assume
#endif

#include "common/data_path_resolver.hpp"
#include "common/file_utils.hpp"
#include "spdlog/spdlog.h"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <vector>

namespace graphics_backend {
namespace {

struct ImGuiConstants {
    float scaleBias[4];
};

constexpr uint32_t kDescriptorSetRingSize = 1024;

} // namespace

ForgeRenderer::ForgeRenderer() {
}

ForgeRenderer::~ForgeRenderer() {
    destroyResources();
    if (fontToken_ != 0) {
        graphics_backend::forge_ui::UnregisterExternalTexture(fontToken_);
        fontToken_ = 0;
    }
    if (uiToken_ != 0) {
        graphics_backend::forge_ui::UnregisterExternalTexture(uiToken_);
        uiToken_ = 0;
    }
    ready_ = false;
    fontsReady_ = false;
}

void* ForgeRenderer::toImGuiTextureId(const graphics::TextureHandle& texture) const {
    if (!texture.valid()) {
        return nullptr;
    }
    return reinterpret_cast<void*>(static_cast<uintptr_t>(texture.id));
}

void ForgeRenderer::rebuildImGuiFonts(ImFontAtlas* atlas) {
    if (!atlas || !ensureReady()) {
        return;
    }
    if (fontToken_ != 0) {
        graphics_backend::forge_ui::UnregisterExternalTexture(fontToken_);
        fontToken_ = 0;
    }
    unsigned char* pixels = nullptr;
    int width = 0;
    int height = 0;
    atlas->GetTexDataAsRGBA32(&pixels, &width, &height);
    if (!pixels || width <= 0 || height <= 0) {
        return;
    }
    if (fontTexture_) {
        removeResource(fontTexture_);
        fontTexture_ = nullptr;
    }
    if (fontToken_ != 0) {
        graphics_backend::forge_ui::UnregisterExternalTexture(fontToken_);
        fontToken_ = 0;
    }

    TextureDesc textureDesc{};
    textureDesc.mArraySize = 1;
    textureDesc.mDepth = 1;
    textureDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
    textureDesc.mFormat = TinyImageFormat_R8G8B8A8_UNORM;
    textureDesc.mHeight = static_cast<uint32_t>(height);
    textureDesc.mMipLevels = 1;
    textureDesc.mSampleCount = SAMPLE_COUNT_1;
    textureDesc.mStartState = RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    textureDesc.mWidth = static_cast<uint32_t>(width);
    textureDesc.pName = "ImGui Forge Font Texture";

    TextureLoadDesc loadDesc{};
    loadDesc.pDesc = &textureDesc;
    loadDesc.ppTexture = &fontTexture_;
    SyncToken token{};
    addResource(&loadDesc, &token);
    waitForToken(&token);

    TextureUpdateDesc updateDesc = { fontTexture_, 0, 1, 0, 1, RESOURCE_STATE_PIXEL_SHADER_RESOURCE };
    beginUpdateResource(&updateDesc);
    TextureSubresourceUpdate subresource = updateDesc.getSubresourceUpdateDesc(0, 0);
    for (uint32_t row = 0; row < subresource.mRowCount; ++row) {
        std::memcpy(subresource.pMappedData + row * subresource.mDstRowStride,
                    pixels + row * subresource.mSrcRowStride,
                    subresource.mSrcRowStride);
    }
    endUpdateResource(&updateDesc);
    if (fence_) {
        FlushResourceUpdateDesc flush{};
        flush.pOutFence = fence_;
        flushResourceUpdates(&flush);
        waitForFences(renderer_, 1, &fence_);
    }

    fontToken_ = graphics_backend::forge_ui::RegisterExternalTexture(fontTexture_);
    atlas->SetTexID(ui::ToImGuiTextureId(fontToken_));
    fontsReady_ = fontTexture_ != nullptr;
}

uint32_t ForgeRenderer::nextDescriptorSetIndex() {
    const uint32_t index = descriptorSetCursor_ % kDescriptorSetRingSize;
    ++descriptorSetCursor_;
    return index;
}

void ForgeRenderer::renderImGuiToTarget(ImDrawData* drawData) {
    if (!drawData || drawData->TotalVtxCount <= 0) {
        return;
    }
    if (!ensureReady() || !uiTarget_ || !pipeline_ || !descriptorSet_ || !uniformBuffer_ || !fontTexture_ || !sampler_) {
        return;
    }

    const ImGuiIO& io = ImGui::GetIO();
    const int fbWidth = static_cast<int>(io.DisplaySize.x * io.DisplayFramebufferScale.x);
    const int fbHeight = static_cast<int>(io.DisplaySize.y * io.DisplayFramebufferScale.y);
    if (fbWidth <= 0 || fbHeight <= 0) {
        return;
    }

    ensureBuffers(static_cast<std::size_t>(drawData->TotalVtxCount) * sizeof(ImDrawVert),
                  static_cast<std::size_t>(drawData->TotalIdxCount) * sizeof(ImDrawIdx));
    if (!vertexBuffer_ || !indexBuffer_) {
        return;
    }

    BufferUpdateDesc vbUpdate = { vertexBuffer_, 0 };
    beginUpdateResource(&vbUpdate);
    BufferUpdateDesc ibUpdate = { indexBuffer_, 0 };
    beginUpdateResource(&ibUpdate);

    ImDrawVert* vtxDst = static_cast<ImDrawVert*>(vbUpdate.pMappedData);
    ImDrawIdx* idxDst = static_cast<ImDrawIdx*>(ibUpdate.pMappedData);
    for (int n = 0; n < drawData->CmdListsCount; ++n) {
        const ImDrawList* cmdList = drawData->CmdLists[n];
        std::memcpy(vtxDst, cmdList->VtxBuffer.Data, static_cast<size_t>(cmdList->VtxBuffer.Size) * sizeof(ImDrawVert));
        std::memcpy(idxDst, cmdList->IdxBuffer.Data, static_cast<size_t>(cmdList->IdxBuffer.Size) * sizeof(ImDrawIdx));
        vtxDst += cmdList->VtxBuffer.Size;
        idxDst += cmdList->IdxBuffer.Size;
    }

    // debug quad removed

    endUpdateResource(&vbUpdate);
    endUpdateResource(&ibUpdate);

    if (fence_) {
        waitForFences(renderer_, 1, &fence_);
    }
    if (cmdPool_) {
        resetCmdPool(renderer_, cmdPool_);
    }
    if (!cmd_) {
        return;
    }

    beginCmd(cmd_);
    descriptorSetCursor_ = 0;

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
    bindDesc.mRenderTargets[0].mClearValue = {0.0f, 0.0f, 0.0f, 0.0f};
    bindDesc.mRenderTargets[0].mOverrideClearValue = 1;
    bindDesc.mDepthStencil.pDepthStencil = nullptr;
    bindDesc.mDepthStencil.mLoadAction = LOAD_ACTION_DONTCARE;
    bindDesc.mDepthStencil.mStoreAction = STORE_ACTION_DONTCARE;
    cmdBindRenderTargets(cmd_, &bindDesc);

    cmdSetViewport(cmd_, 0.0f, 0.0f, static_cast<float>(fbWidth), static_cast<float>(fbHeight), 0.0f, 1.0f);
    cmdSetScissor(cmd_, 0, 0, static_cast<uint32_t>(fbWidth), static_cast<uint32_t>(fbHeight));

    ImGuiConstants constants{};
    const float sx = 2.0f / io.DisplaySize.x;
    const float sy = -2.0f / io.DisplaySize.y;
    const float tx = -1.0f - drawData->DisplayPos.x * sx;
    const float ty = 1.0f + drawData->DisplayPos.y * sy;
    constants.scaleBias[0] = sx;
    constants.scaleBias[1] = sy;
    constants.scaleBias[2] = tx;
    constants.scaleBias[3] = ty;

    BufferUpdateDesc cbUpdate = { uniformBuffer_ };
    beginUpdateResource(&cbUpdate);
    std::memcpy(cbUpdate.pMappedData, &constants, sizeof(constants));
    endUpdateResource(&cbUpdate);

    DescriptorData baseParams[3] = {};
    baseParams[0].mIndex = 0;
    baseParams[0].ppBuffers = &uniformBuffer_;
    baseParams[2].mIndex = 2;
    baseParams[2].ppSamplers = &sampler_;

    cmdBindPipeline(cmd_, pipeline_);
    uint32_t stride = sizeof(ImDrawVert);
    uint64_t offset = 0;
    cmdBindVertexBuffer(cmd_, 1, &vertexBuffer_, &stride, &offset);
    cmdBindIndexBuffer(cmd_, indexBuffer_, sizeof(ImDrawIdx) == 2 ? INDEX_TYPE_UINT16 : INDEX_TYPE_UINT32, 0);

    drawData->ScaleClipRects(io.DisplayFramebufferScale);

    int globalVtxOffset = 0;
    int globalIdxOffset = 0;
    for (int n = 0; n < drawData->CmdListsCount; ++n) {
        const ImDrawList* cmdList = drawData->CmdLists[n];
        for (int cmdIdx = 0; cmdIdx < cmdList->CmdBuffer.Size; ++cmdIdx) {
            const ImDrawCmd* pcmd = &cmdList->CmdBuffer[cmdIdx];
            if (pcmd->UserCallback) {
                pcmd->UserCallback(cmdList, pcmd);
                continue;
            }
            const ImVec4 clip = pcmd->ClipRect;
            const float clipX = std::max(0.0f, clip.x);
            const float clipY = std::max(0.0f, clip.y);
            const float clipZ = std::min(clip.z, static_cast<float>(fbWidth));
            const float clipW = std::min(clip.w, static_cast<float>(fbHeight));
            const float clipWf = clipZ - clipX;
            const float clipHf = clipW - clipY;
            if (clipWf <= 0.0f || clipHf <= 0.0f) {
                continue;
            }
            cmdSetScissor(cmd_, static_cast<uint32_t>(clipX), static_cast<uint32_t>(clipY),
                          static_cast<uint32_t>(clipWf), static_cast<uint32_t>(clipHf));

            uint64_t token = pcmd->TextureId ? ui::FromImGuiTextureId(pcmd->TextureId) : fontToken_;
            Texture* texture = static_cast<Texture*>(graphics_backend::forge_ui::ResolveExternalTexture(token));
            if (!texture) {
                texture = fontTexture_;
            }
            DescriptorData params[3] = {};
            params[0] = baseParams[0];
            params[1].mIndex = 1;
            params[1].ppTextures = &texture;
            params[2] = baseParams[2];
            const uint32_t setIndex = nextDescriptorSetIndex();
            updateDescriptorSet(renderer_, setIndex, descriptorSet_, 3, params);
            cmdBindDescriptorSet(cmd_, setIndex, descriptorSet_);

            cmdDrawIndexed(cmd_, pcmd->ElemCount, static_cast<uint32_t>(globalIdxOffset + pcmd->IdxOffset),
                           static_cast<uint32_t>(globalVtxOffset + pcmd->VtxOffset));
        }
        globalIdxOffset += cmdList->IdxBuffer.Size;
        globalVtxOffset += cmdList->VtxBuffer.Size;
    }

    // debug draw removed

    cmdBindRenderTargets(cmd_, nullptr);
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
}

bool ForgeRenderer::isImGuiReady() const {
    return ready_ && fontsReady_;
}

void ForgeRenderer::ensureImGuiRenderTarget(int width, int height) {
    if (!ensureReady()) {
        return;
    }
    if (width <= 0 || height <= 0) {
        if (uiToken_ != 0) {
            graphics_backend::forge_ui::UnregisterExternalTexture(uiToken_);
            uiToken_ = 0;
        }
        uiWidth_ = 0;
        uiHeight_ = 0;
        return;
    }
    if (width == uiWidth_ && height == uiHeight_) {
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
    rtDesc.pName = "ImGui Forge UI RT";
    addRenderTarget(renderer_, &rtDesc, &uiTarget_);

    if (uiTarget_ && uiTarget_->pTexture) {
        uiToken_ = graphics_backend::forge_ui::RegisterExternalTexture(uiTarget_->pTexture);
    }
    uiWidth_ = width;
    uiHeight_ = height;
}

graphics::TextureHandle ForgeRenderer::getImGuiRenderTarget() const {
    graphics::TextureHandle handle{};
    if (uiToken_ == 0) {
        return handle;
    }
    handle.id = uiToken_;
    handle.width = static_cast<uint32_t>(uiWidth_);
    handle.height = static_cast<uint32_t>(uiHeight_);
    handle.format = graphics::TextureFormat::RGBA8_UNORM;
    return handle;
}

bool ForgeRenderer::ensureReady() {
    if (ready_) {
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
    ready_ = renderer_ != nullptr;
    return ready_;
}

void ForgeRenderer::ensurePipeline() {
    if (!renderer_ || pipeline_) {
        return;
    }

    const std::filesystem::path shaderDir = karma::data::Resolve("forge/shaders");
    const auto vsPath = shaderDir / "imgui.vert.spv";
    const auto fsPath = shaderDir / "imgui.frag.spv";
    auto vsBytes = karma::file::ReadFileBytes(vsPath);
    auto fsBytes = karma::file::ReadFileBytes(fsPath);
    if (vsBytes.empty() || fsBytes.empty()) {
        spdlog::error("UiSystem(Forge): missing ImGui shaders '{}', '{}'", vsPath.string(), fsPath.string());
        return;
    }

    BinaryShaderDesc shaderDesc{};
    shaderDesc.mStages = SHADER_STAGE_VERT | SHADER_STAGE_FRAG;
    shaderDesc.mOwnByteCode = false;
    shaderDesc.mVert = { "imgui.vert", vsBytes.data(), static_cast<uint32_t>(vsBytes.size()), "main" };
    shaderDesc.mFrag = { "imgui.frag", fsBytes.data(), static_cast<uint32_t>(fsBytes.size()), "main" };
    addShaderBinary(renderer_, &shaderDesc, &shader_);
    if (!shader_) {
        spdlog::error("UiSystem(Forge): failed to create ImGui shader");
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
    setDesc.mMaxSets = kDescriptorSetRingSize;
    setDesc.mDescriptorCount = 3;
    setDesc.pDescriptors = descriptors_;
    addDescriptorSet(renderer_, &setDesc, &descriptorSet_);

    BufferLoadDesc ubDesc{};
    ubDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    ubDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
    ubDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
    ubDesc.mDesc.mStartState = RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
    ubDesc.mDesc.mSize = sizeof(ImGuiConstants);
    ubDesc.mDesc.pName = "ImGui Forge Uniform";
    ubDesc.ppBuffer = &uniformBuffer_;
    addResource(&ubDesc, nullptr);

    VertexLayout layout{};
    layout.mBindingCount = 1;
    layout.mAttribCount = 3;
    layout.mBindings[0].mStride = sizeof(ImDrawVert);
    layout.mBindings[0].mRate = VERTEX_BINDING_RATE_VERTEX;
    layout.mAttribs[0].mSemantic = SEMANTIC_POSITION;
    layout.mAttribs[0].mFormat = TinyImageFormat_R32G32_SFLOAT;
    layout.mAttribs[0].mBinding = 0;
    layout.mAttribs[0].mLocation = 0;
    layout.mAttribs[0].mOffset = IM_OFFSETOF(ImDrawVert, pos);
    layout.mAttribs[1].mSemantic = SEMANTIC_TEXCOORD0;
    layout.mAttribs[1].mFormat = TinyImageFormat_R32G32_SFLOAT;
    layout.mAttribs[1].mBinding = 0;
    layout.mAttribs[1].mLocation = 1;
    layout.mAttribs[1].mOffset = IM_OFFSETOF(ImDrawVert, uv);
    layout.mAttribs[2].mSemantic = SEMANTIC_COLOR;
    layout.mAttribs[2].mFormat = TinyImageFormat_R8G8B8A8_UNORM;
    layout.mAttribs[2].mBinding = 0;
    layout.mAttribs[2].mLocation = 2;
    layout.mAttribs[2].mOffset = IM_OFFSETOF(ImDrawVert, col);

    BlendStateDesc blend{};
    blend.mSrcFactors[0] = BC_SRC_ALPHA;
    blend.mDstFactors[0] = BC_ONE_MINUS_SRC_ALPHA;
    blend.mSrcAlphaFactors[0] = BC_SRC_ALPHA;
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
}

void ForgeRenderer::ensureBuffers(std::size_t vertexBytes, std::size_t indexBytes) {
    if (!renderer_) {
        return;
    }
    if (!vertexBuffer_ || vertexBufferSize_ < vertexBytes) {
        if (vertexBuffer_) {
            removeResource(vertexBuffer_);
            vertexBuffer_ = nullptr;
        }
    BufferLoadDesc vbDesc{};
    vbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
    vbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
    vbDesc.mDesc.mSize = std::max<std::size_t>(vertexBytes, 1024u);
    vbDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
    vbDesc.mDesc.mStartState = RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
    vbDesc.mDesc.pName = "ImGui Forge VB";
    vbDesc.ppBuffer = &vertexBuffer_;
    addResource(&vbDesc, nullptr);
        vertexBufferSize_ = vbDesc.mDesc.mSize;
    }

    if (!indexBuffer_ || indexBufferSize_ < indexBytes) {
        if (indexBuffer_) {
            removeResource(indexBuffer_);
            indexBuffer_ = nullptr;
        }
    BufferLoadDesc ibDesc{};
    ibDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_INDEX_BUFFER;
    ibDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
    ibDesc.mDesc.mSize = std::max<std::size_t>(indexBytes, 1024u);
    ibDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
    ibDesc.mDesc.mStartState = RESOURCE_STATE_INDEX_BUFFER;
    ibDesc.mDesc.pName = "ImGui Forge IB";
    ibDesc.ppBuffer = &indexBuffer_;
    addResource(&ibDesc, nullptr);
        indexBufferSize_ = ibDesc.mDesc.mSize;
    }
}

void ForgeRenderer::destroyResources() {
    if (renderer_) {
        if (pipeline_) {
            removePipeline(renderer_, pipeline_);
        }
        if (shader_) {
            removeShader(renderer_, shader_);
        }
        if (descriptorSet_) {
            removeDescriptorSet(renderer_, descriptorSet_);
        }
        if (sampler_) {
            removeSampler(renderer_, sampler_);
        }
        if (vertexBuffer_) {
            removeResource(vertexBuffer_);
        }
        if (indexBuffer_) {
            removeResource(indexBuffer_);
        }
        if (uniformBuffer_) {
            removeResource(uniformBuffer_);
        }
        if (fontTexture_) {
            removeResource(fontTexture_);
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
    vertexBuffer_ = nullptr;
    indexBuffer_ = nullptr;
    uniformBuffer_ = nullptr;
    fontTexture_ = nullptr;
    uiTarget_ = nullptr;
    cmd_ = nullptr;
    cmdPool_ = nullptr;
    fence_ = nullptr;
    vertexBufferSize_ = 0;
    indexBufferSize_ = 0;
    ready_ = false;
    fontsReady_ = false;
}

} // namespace graphics_backend
