#include "engine/graphics/backends/forge/backend.hpp"

#include "engine/graphics/backends/forge/ui_bridge.hpp"
#include "engine/geometry/mesh_loader.hpp"
#include "common/data_path_resolver.hpp"
#include "platform/window.hpp"
#include "spdlog/spdlog.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#if defined(BZ3_WINDOW_BACKEND_SDL3)
#include <SDL3/SDL.h>
#include <SDL3/SDL_properties.h>
#include <SDL3/SDL_video.h>
#endif

#define IMEMORY_FROM_HEADER
#include "Common_3/Utilities/Interfaces/IMemory.h"
#include "Common_3/Utilities/Interfaces/IFileSystem.h"
#include "Common_3/Utilities/Interfaces/ILog.h"
#include "Common_3/Resources/ResourceLoader/Interfaces/IResourceLoader.h"

#include <filesystem>
#include <fstream>
#include <vector>
#include <cstring>

namespace graphics_backend {

namespace {

WindowHandle buildWindowHandle(platform::Window* window) {
    WindowHandle handle{};
    handle.type = WINDOW_HANDLE_TYPE_UNKNOWN;
    if (!window) {
        return handle;
    }
#if defined(BZ3_WINDOW_BACKEND_SDL3)
    auto* sdlWindow = static_cast<SDL_Window*>(window->nativeHandle());
    if (!sdlWindow) {
        return handle;
    }
    const SDL_PropertiesID props = SDL_GetWindowProperties(sdlWindow);
    if (props == 0) {
        spdlog::warn("Graphics(Forge): SDL_GetWindowProperties failed: {}", SDL_GetError());
        return handle;
    }
    if (void* wlDisplay = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER, nullptr)) {
        void* wlSurface = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, nullptr);
        if (wlSurface) {
            handle.type = WINDOW_HANDLE_TYPE_WAYLAND;
            handle.wl_display = static_cast<struct wl_display*>(wlDisplay);
            handle.wl_surface = static_cast<struct wl_surface*>(wlSurface);
            return handle;
        }
    }
#endif
    return handle;
}

std::filesystem::path resolveRepoRoot() {
    auto cwd = std::filesystem::current_path();
    if (std::filesystem::exists(cwd / "CMakeLists.txt")) {
        return cwd;
    }
    if (cwd.filename().string().rfind("build-", 0) == 0) {
        auto parent = cwd.parent_path();
        if (std::filesystem::exists(parent / "CMakeLists.txt")) {
            return parent;
        }
    }
    return cwd;
}

void ensureForgeConfigFiles() {
    const auto repoRoot = resolveRepoRoot();
    const auto forgeDataDir = repoRoot / "forge_data";
    std::error_code ec;
    std::filesystem::create_directories(forgeDataDir, ec);

    const auto gpuCfgPath = forgeDataDir / "gpu.cfg";
    const auto gpuDataPath = forgeDataDir / "gpu.data";
    if (!std::filesystem::exists(gpuCfgPath)) {
        std::ofstream out(gpuCfgPath);
        out << "version: 0.3\n"
               "#Possible Classfications for Preset: ultra; high; medium; low; verylow; office\n"
               "BEGIN_DEFAULT_CONFIGURATION;\n"
               "DefaultPresetLevel; medium;\n"
               "END_DEFAULT_CONFIGURATION;\n"
               "BEGIN_VENDOR_LIST;\n"
               "intel; 0x8086;\n"
               "amd; 0x1002;\n"
               "nvidia; 0x10de;\n"
               "END_VENDOR_LIST;\n"
               "BEGIN_GPU_LIST;\n"
               "0x10de; 0x13c2; medium; #NVIDIA; NVIDIA GeForce GTX 970\n"
               "END_GPU_LIST;\n";
    }
    if (!std::filesystem::exists(gpuDataPath)) {
        std::ofstream out(gpuDataPath);
        out << "version: 0.3\n"
               "BEGIN_DEFAULT_CONFIGURATION;\n"
               "DefaultPresetLevel; medium;\n"
               "END_DEFAULT_CONFIGURATION;\n"
               "BEGIN_VENDOR_LIST;\n"
               "intel; 0x8086;\n"
               "amd; 0x1002;\n"
               "nvidia; 0x10de;\n"
               "END_VENDOR_LIST;\n"
               "BEGIN_GPU_LIST;\n"
               "0x10de; 0x13c2; medium; #NVIDIA; NVIDIA GeForce GTX 970\n"
               "END_GPU_LIST;\n";
    }

    const auto pathStatementPath = std::filesystem::current_path() / "PathStatement.txt";
    {
        std::ofstream out(pathStatementPath, std::ios::trunc);
        out << "RD_GPU_CONFIG = " << forgeDataDir.string() << "\n"
               "RD_OTHER_FILES = " << forgeDataDir.string() << "\n"
               "RD_LOG = " << forgeDataDir.string() << "\n";
    }
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

struct MeshVertex {
    float px, py, pz;
    float nx, ny, nz;
    float u, v;
};

struct MeshConstants {
    float mvp[16];
    float color[4];
};

} // namespace

ForgeBackend::ForgeBackend(platform::Window& windowRef) : window(&windowRef) {
    spdlog::info("Graphics(Forge): init begin");
    int fbWidth = 0;
    int fbHeight = 0;
    window->getFramebufferSize(fbWidth, fbHeight);
    framebufferWidth = fbWidth;
    framebufferHeight = fbHeight;

    initMemAlloc("bz3");
    spdlog::info("Graphics(Forge): initMemAlloc ok");
    ensureForgeConfigFiles();
    FileSystemInitDesc fsDesc{};
    fsDesc.pAppName = "bz3";
    if (!initFileSystem(&fsDesc)) {
        spdlog::error("Graphics(Forge): initFileSystem failed");
        return;
    }
    initLog("bz3", eALL);
    spdlog::info("Graphics(Forge): initFileSystem/initLog ok");
    initGPUConfiguration(nullptr);
    spdlog::info("Graphics(Forge): initGPUConfiguration ok");

    RendererDesc rendererDesc{};
    rendererDesc.mGpuMode = GPU_MODE_SINGLE;
    rendererDesc.mShaderTarget = SHADER_TARGET_6_0;
    initRenderer("bz3", &rendererDesc, &renderer_);
    if (!renderer_) {
        spdlog::error("Graphics(Forge): failed to initialize renderer.");
        return;
    }
    spdlog::info("Graphics(Forge): initRenderer ok");
    initResourceLoaderInterface(renderer_, nullptr);
    setupGPUConfigurationPlatformParameters(renderer_, nullptr);
    spdlog::info("Graphics(Forge): setupGPUConfigurationPlatformParameters ok");

    QueueDesc queueDesc{};
    queueDesc.mType = QUEUE_TYPE_GRAPHICS;
    queueDesc.mPriority = QUEUE_PRIORITY_NORMAL;
    initQueue(renderer_, &queueDesc, &graphicsQueue_);
    if (!graphicsQueue_) {
        spdlog::error("Graphics(Forge): failed to create graphics queue.");
        return;
    }
    spdlog::info("Graphics(Forge): initQueue ok");

    WindowHandle handle = buildWindowHandle(window);
    if (handle.type == WINDOW_HANDLE_TYPE_UNKNOWN) {
        spdlog::error("Graphics(Forge): unsupported SDL3 native window handle.");
        return;
    }
    spdlog::info("Graphics(Forge): buildWindowHandle ok type={}", static_cast<int>(handle.type));

    SwapChainDesc swapDesc{};
    swapDesc.mWindowHandle = handle;
    swapDesc.mPresentQueueCount = 1;
    swapDesc.ppPresentQueues = &graphicsQueue_;
    swapDesc.mWidth = static_cast<uint32_t>(framebufferWidth);
    swapDesc.mHeight = static_cast<uint32_t>(framebufferHeight);
    swapDesc.mEnableVsync = true;
    swapDesc.mImageCount = getRecommendedSwapchainImageCount(renderer_, &handle);
    swapDesc.mColorFormat = getSupportedSwapchainFormat(renderer_, &swapDesc, COLOR_SPACE_SDR_SRGB);
    addSwapChain(renderer_, &swapDesc, &swapChain_);
    if (!swapChain_) {
        spdlog::error("Graphics(Forge): failed to create swapchain.");
        return;
    }
    spdlog::info("Graphics(Forge): addSwapChain ok");

    initFence(renderer_, &renderFence_);
    initSemaphore(renderer_, &imageAcquiredSemaphore_);
    initSemaphore(renderer_, &renderCompleteSemaphore_);
    CmdPoolDesc cmdPoolDesc{};
    cmdPoolDesc.pQueue = graphicsQueue_;
    initCmdPool(renderer_, &cmdPoolDesc, &cmdPool_);
    CmdDesc cmdDesc{};
    cmdDesc.pPool = cmdPool_;
    initCmd(renderer_, &cmdDesc, &cmd_);
    spdlog::info("Graphics(Forge): sync primitives ok");

    const uint32_t colorFormat = swapChain_->ppRenderTargets[0]->mFormat;
    graphics_backend::forge_ui::SetContext(renderer_, graphicsQueue_, framebufferWidth, framebufferHeight, colorFormat);
    spdlog::info("Graphics(Forge): ui bridge context ok");

#if defined(BZ3_UI_BACKEND_IMGUI)
    uiBridge_ = std::make_unique<ForgeRenderer>();
#endif
    spdlog::info("Graphics(Forge): initialized (SDL3 + Vulkan).");
}

ForgeBackend::~ForgeBackend() {
    if (graphicsQueue_) {
        waitQueueIdle(graphicsQueue_);
    }
    uiBridge_.reset();
    destroyUiOverlayResources();
    for (auto& [id, mesh] : meshes) {
        if (mesh.vertexBuffer) {
            removeResource(mesh.vertexBuffer);
            mesh.vertexBuffer = nullptr;
        }
        if (mesh.indexBuffer) {
            removeResource(mesh.indexBuffer);
            mesh.indexBuffer = nullptr;
        }
    }
    meshes.clear();
    modelMeshCache.clear();
    destroyMeshResources();
    if (swapChain_) {
        removeSwapChain(renderer_, swapChain_);
        swapChain_ = nullptr;
    }
    if (renderFence_) {
        exitFence(renderer_, renderFence_);
        renderFence_ = nullptr;
    }
    if (imageAcquiredSemaphore_) {
        exitSemaphore(renderer_, imageAcquiredSemaphore_);
        imageAcquiredSemaphore_ = nullptr;
    }
    if (renderCompleteSemaphore_) {
        exitSemaphore(renderer_, renderCompleteSemaphore_);
        renderCompleteSemaphore_ = nullptr;
    }
    if (cmd_) {
        exitCmd(renderer_, cmd_);
        cmd_ = nullptr;
    }
    if (cmdPool_) {
        exitCmdPool(renderer_, cmdPool_);
        cmdPool_ = nullptr;
    }
    if (graphicsQueue_) {
        exitQueue(renderer_, graphicsQueue_);
        graphicsQueue_ = nullptr;
    }
    if (renderer_) {
        exitResourceLoaderInterface(renderer_);
        exitRenderer(renderer_);
        renderer_ = nullptr;
    }
    exitGPUConfiguration();
    removeGPUConfigurationRules();
    exitLog();
    exitFileSystem();
    exitMemAlloc();
    graphics_backend::forge_ui::ClearContext();
}

void ForgeBackend::beginFrame() {
    if (!renderer_ || !swapChain_) {
        return;
    }
    if (renderFence_) {
        waitForFences(renderer_, 1, &renderFence_);
    }
    if (cmdPool_) {
        resetCmdPool(renderer_, cmdPool_);
    }
    acquireNextImage(renderer_, swapChain_, imageAcquiredSemaphore_, renderFence_, &frameIndex_);
    if (!cmd_) {
        return;
    }
    beginCmd(cmd_);
    RenderTarget* backBuffer = swapChain_->ppRenderTargets[frameIndex_];
    BindRenderTargetsDesc bindDesc{};
    bindDesc.mRenderTargetCount = 1;
    bindDesc.mRenderTargets[0].pRenderTarget = backBuffer;
    bindDesc.mRenderTargets[0].mLoadAction = LOAD_ACTION_CLEAR;
    bindDesc.mRenderTargets[0].mStoreAction = STORE_ACTION_STORE;
    bindDesc.mRenderTargets[0].mClearValue = {0.05f, 0.08f, 0.12f, 1.0f};
    bindDesc.mRenderTargets[0].mOverrideClearValue = 1;
    bindDesc.mDepthStencil.pDepthStencil = nullptr;
    bindDesc.mDepthStencil.mLoadAction = LOAD_ACTION_DONTCARE;
    bindDesc.mDepthStencil.mStoreAction = STORE_ACTION_DONTCARE;
    cmdBindRenderTargets(cmd_, &bindDesc);
    cmdSetViewport(cmd_, 0.0f, 0.0f, static_cast<float>(framebufferWidth),
                   static_cast<float>(framebufferHeight), 0.0f, 1.0f);
    cmdSetScissor(cmd_, 0, 0, static_cast<uint32_t>(framebufferWidth),
                  static_cast<uint32_t>(framebufferHeight));
}

void ForgeBackend::endFrame() {
    if (!graphicsQueue_ || !swapChain_) {
        return;
    }
    if (cmd_) {
        endCmd(cmd_);
        QueueSubmitDesc submitDesc{};
        submitDesc.ppCmds = &cmd_;
        submitDesc.mCmdCount = 1;
        submitDesc.pSignalFence = renderFence_;
        submitDesc.ppWaitSemaphores = &imageAcquiredSemaphore_;
        submitDesc.mWaitSemaphoreCount = 1;
        submitDesc.ppSignalSemaphores = &renderCompleteSemaphore_;
        submitDesc.mSignalSemaphoreCount = 1;
        queueSubmit(graphicsQueue_, &submitDesc);
    }
    QueuePresentDesc presentDesc{};
    presentDesc.pSwapChain = swapChain_;
    presentDesc.ppWaitSemaphores = &renderCompleteSemaphore_;
    presentDesc.mWaitSemaphoreCount = 1;
    presentDesc.mIndex = static_cast<uint8_t>(frameIndex_);
    queuePresent(graphicsQueue_, &presentDesc);
}

void ForgeBackend::resize(int width, int height) {
    if (width == framebufferWidth && height == framebufferHeight && swapChain_) {
        return;
    }
    framebufferWidth = width;
    framebufferHeight = height;
    auto ctx = graphics_backend::forge_ui::GetContext();
    graphics_backend::forge_ui::SetContext(ctx.renderer, ctx.graphicsQueue, width, height, ctx.colorFormat);
    if (!renderer_ || !swapChain_) {
        return;
    }
    waitQueueIdle(graphicsQueue_);
    removeSwapChain(renderer_, swapChain_);
    swapChain_ = nullptr;
    WindowHandle handle = buildWindowHandle(window);
    if (handle.type == WINDOW_HANDLE_TYPE_UNKNOWN) {
        return;
    }
    SwapChainDesc swapDesc{};
    swapDesc.mWindowHandle = handle;
    swapDesc.mPresentQueueCount = 1;
    swapDesc.ppPresentQueues = &graphicsQueue_;
    swapDesc.mWidth = static_cast<uint32_t>(framebufferWidth);
    swapDesc.mHeight = static_cast<uint32_t>(framebufferHeight);
    swapDesc.mEnableVsync = true;
    swapDesc.mImageCount = getRecommendedSwapchainImageCount(renderer_, &handle);
    swapDesc.mColorFormat = getSupportedSwapchainFormat(renderer_, &swapDesc, COLOR_SPACE_SDR_SRGB);
    addSwapChain(renderer_, &swapDesc, &swapChain_);
    if (swapChain_) {
        const uint32_t colorFormat = swapChain_->ppRenderTargets[0]->mFormat;
        graphics_backend::forge_ui::SetContext(renderer_, graphicsQueue_, framebufferWidth, framebufferHeight, colorFormat);
    }
}

graphics::EntityId ForgeBackend::createEntity(graphics::LayerId layer) {
    const auto id = nextEntityId++;
    entities[id] = EntityRecord{layer};
    return id;
}

graphics::EntityId ForgeBackend::createModelEntity(const std::filesystem::path& modelPath,
                                                   graphics::LayerId layer,
                                                   graphics::MaterialId materialOverride) {
    const auto id = createEntity(layer);
    setEntityModel(id, modelPath, materialOverride);
    return id;
}

graphics::EntityId ForgeBackend::createMeshEntity(graphics::MeshId mesh,
                                                  graphics::LayerId layer,
                                                  graphics::MaterialId materialOverride) {
    const auto id = createEntity(layer);
    setEntityMesh(id, mesh, materialOverride);
    return id;
}

void ForgeBackend::setEntityModel(graphics::EntityId entity,
                                  const std::filesystem::path& modelPath,
                                  graphics::MaterialId materialOverride) {
    auto it = entities.find(entity);
    if (it == entities.end()) {
        return;
    }
    it->second.modelPath = modelPath;
    it->second.material = materialOverride;

    const std::string pathKey = modelPath.string();
    auto cacheIt = modelMeshCache.find(pathKey);
    if (cacheIt != modelMeshCache.end()) {
        it->second.meshes = cacheIt->second;
        it->second.mesh = cacheIt->second.empty() ? graphics::kInvalidMesh : cacheIt->second.front();
        return;
    }

    const auto resolved = bz::data::Resolve(modelPath);
    MeshLoader::LoadOptions options;
    options.loadTextures = false;
    auto loaded = MeshLoader::loadGLB(resolved.string(), options);
    if (loaded.empty()) {
        return;
    }

    std::vector<graphics::MeshId> modelMeshes;
    modelMeshes.reserve(loaded.size());
    for (const auto& submesh : loaded) {
        graphics::MeshData meshData;
        meshData.vertices = submesh.vertices;
        meshData.indices = submesh.indices;
        meshData.texcoords = submesh.texcoords;
        meshData.normals = submesh.normals;

        const graphics::MeshId meshId = createMesh(meshData);
        if (meshId == graphics::kInvalidMesh) {
            continue;
        }
        modelMeshes.push_back(meshId);
    }

    it->second.meshes = modelMeshes;
    it->second.mesh = modelMeshes.empty() ? graphics::kInvalidMesh : modelMeshes.front();
    modelMeshCache.emplace(pathKey, std::move(modelMeshes));
}

void ForgeBackend::setEntityMesh(graphics::EntityId entity,
                                 graphics::MeshId mesh,
                                 graphics::MaterialId materialOverride) {
    auto it = entities.find(entity);
    if (it == entities.end()) {
        return;
    }
    it->second.mesh = mesh;
    it->second.meshes.clear();
    it->second.material = materialOverride;
}

void ForgeBackend::destroyEntity(graphics::EntityId entity) {
    entities.erase(entity);
}

graphics::MeshId ForgeBackend::createMesh(const graphics::MeshData& mesh) {
    const auto id = nextMeshId++;
    if (!renderer_) {
        meshes[id] = MeshRecord{};
        return id;
    }

    ensureMeshResources();
    MeshRecord record;
    if (mesh.vertices.empty() || mesh.indices.empty()) {
        meshes[id] = record;
        return id;
    }

    std::vector<glm::vec3> normals = mesh.normals;
    if (normals.size() != mesh.vertices.size()) {
        normals.assign(mesh.vertices.size(), glm::vec3(0.0f));
        if (mesh.indices.size() >= 3) {
            for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
                const uint32_t i0 = mesh.indices[i];
                const uint32_t i1 = mesh.indices[i + 1];
                const uint32_t i2 = mesh.indices[i + 2];
                if (i0 >= mesh.vertices.size() || i1 >= mesh.vertices.size() || i2 >= mesh.vertices.size()) {
                    continue;
                }
                const glm::vec3& v0 = mesh.vertices[i0];
                const glm::vec3& v1 = mesh.vertices[i1];
                const glm::vec3& v2 = mesh.vertices[i2];
                const glm::vec3 n = glm::normalize(glm::cross(v1 - v0, v2 - v0));
                normals[i0] += n;
                normals[i1] += n;
                normals[i2] += n;
            }
            for (auto& n : normals) {
                if (glm::length(n) > 0.0f) {
                    n = glm::normalize(n);
                } else {
                    n = glm::vec3(0.0f, 1.0f, 0.0f);
                }
            }
        } else {
            for (auto& n : normals) {
                n = glm::vec3(0.0f, 1.0f, 0.0f);
            }
        }
    }

    std::vector<MeshVertex> packed;
    packed.resize(mesh.vertices.size());
    for (size_t i = 0; i < mesh.vertices.size(); ++i) {
        const auto& v = mesh.vertices[i];
        const auto& n = normals[i];
        const glm::vec2 uv = (i < mesh.texcoords.size()) ? mesh.texcoords[i] : glm::vec2(0.0f);
        packed[i] = {v.x, v.y, v.z, n.x, n.y, n.z, uv.x, uv.y};
    }

    BufferLoadDesc vbDesc{};
    vbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
    vbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
    vbDesc.mDesc.mSize = packed.size() * sizeof(MeshVertex);
    vbDesc.mDesc.pName = "Forge Mesh VB";
    vbDesc.pData = packed.data();
    vbDesc.ppBuffer = &record.vertexBuffer;
    SyncToken vbToken{};
    addResource(&vbDesc, &vbToken);
    waitForToken(&vbToken);

    BufferLoadDesc ibDesc{};
    ibDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_INDEX_BUFFER;
    ibDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
    ibDesc.mDesc.mSize = mesh.indices.size() * sizeof(uint32_t);
    ibDesc.mDesc.pName = "Forge Mesh IB";
    ibDesc.pData = mesh.indices.data();
    ibDesc.ppBuffer = &record.indexBuffer;
    SyncToken ibToken{};
    addResource(&ibDesc, &ibToken);
    waitForToken(&ibToken);

    record.indexCount = static_cast<uint32_t>(mesh.indices.size());
    record.texture = whiteTexture_;
    meshes[id] = record;
    return id;
}

void ForgeBackend::destroyMesh(graphics::MeshId mesh) {
    auto it = meshes.find(mesh);
    if (it == meshes.end()) {
        return;
    }
    if (it->second.vertexBuffer) {
        removeResource(it->second.vertexBuffer);
        it->second.vertexBuffer = nullptr;
    }
    if (it->second.indexBuffer) {
        removeResource(it->second.indexBuffer);
        it->second.indexBuffer = nullptr;
    }
    meshes.erase(it);
}

graphics::MaterialId ForgeBackend::createMaterial(const graphics::MaterialDesc& material) {
    const auto id = nextMaterialId++;
    materials[id] = material;
    return id;
}

void ForgeBackend::updateMaterial(graphics::MaterialId material, const graphics::MaterialDesc& desc) {
    auto it = materials.find(material);
    if (it == materials.end()) {
        return;
    }
    it->second = desc;
}

void ForgeBackend::destroyMaterial(graphics::MaterialId material) {
    materials.erase(material);
}

void ForgeBackend::setMaterialFloat(graphics::MaterialId material, std::string_view name, float value) {
    (void)material;
    (void)name;
    (void)value;
}

graphics::RenderTargetId ForgeBackend::createRenderTarget(const graphics::RenderTargetDesc& desc) {
    const auto id = nextRenderTargetId++;
    RenderTargetRecord record{};
    record.desc = desc;
    if (renderer_ && desc.width > 0 && desc.height > 0) {
        RenderTargetDesc rtDesc{};
        rtDesc.mWidth = static_cast<uint32_t>(desc.width);
        rtDesc.mHeight = static_cast<uint32_t>(desc.height);
        rtDesc.mDepth = 1;
        rtDesc.mArraySize = 1;
        rtDesc.mMipLevels = 1;
        rtDesc.mSampleCount = SAMPLE_COUNT_1;
        rtDesc.mSampleQuality = 0;
        rtDesc.mFormat = TinyImageFormat_R8G8B8A8_UNORM;
        rtDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
        rtDesc.mStartState = RESOURCE_STATE_RENDER_TARGET;
        rtDesc.pName = "Forge RenderTarget";
        addRenderTarget(renderer_, &rtDesc, &record.renderTarget);
        if (record.renderTarget && record.renderTarget->pTexture) {
            record.token = graphics_backend::forge_ui::RegisterExternalTexture(record.renderTarget->pTexture);
        }
    }
    renderTargets[id] = record;
    return id;
}

void ForgeBackend::destroyRenderTarget(graphics::RenderTargetId target) {
    auto it = renderTargets.find(target);
    if (it == renderTargets.end()) {
        return;
    }
    if (it->second.token != 0) {
        graphics_backend::forge_ui::UnregisterExternalTexture(it->second.token);
    }
    if (renderer_ && it->second.renderTarget) {
        removeRenderTarget(renderer_, it->second.renderTarget);
    }
    renderTargets.erase(it);
}

void ForgeBackend::renderLayer(graphics::LayerId layer, graphics::RenderTargetId target) {
    if (!renderer_ || !cmd_) {
        return;
    }
    ensureMeshResources();

    RenderTarget* renderTarget = nullptr;
    int targetWidth = framebufferWidth;
    int targetHeight = framebufferHeight;
    const bool useSwapchain = (target == graphics::kDefaultRenderTarget);
    if (useSwapchain) {
        if (!swapChain_) {
            return;
        }
        renderTarget = swapChain_->ppRenderTargets[frameIndex_];
    } else {
        auto it = renderTargets.find(target);
        if (it == renderTargets.end() || !it->second.renderTarget) {
            return;
        }
        renderTarget = it->second.renderTarget;
        targetWidth = it->second.desc.width;
        targetHeight = it->second.desc.height;
        RenderTargetBarrier rtBegin{};
        rtBegin.pRenderTarget = renderTarget;
        rtBegin.mCurrentState = RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        rtBegin.mNewState = RESOURCE_STATE_RENDER_TARGET;
        cmdResourceBarrier(cmd_, 0, nullptr, 0, nullptr, 1, &rtBegin);
    }

    BindRenderTargetsDesc bindDesc{};
    bindDesc.mRenderTargetCount = 1;
    bindDesc.mRenderTargets[0].pRenderTarget = renderTarget;
    bindDesc.mRenderTargets[0].mLoadAction = LOAD_ACTION_CLEAR;
    bindDesc.mRenderTargets[0].mStoreAction = STORE_ACTION_STORE;
    if (useSwapchain) {
        bindDesc.mRenderTargets[0].mClearValue = {0.05f, 0.08f, 0.12f, 1.0f};
    } else {
        bindDesc.mRenderTargets[0].mClearValue = {0.0f, 0.0f, 0.0f, 0.0f};
    }
    bindDesc.mRenderTargets[0].mOverrideClearValue = 1;
    bindDesc.mDepthStencil.pDepthStencil = nullptr;
    bindDesc.mDepthStencil.mLoadAction = LOAD_ACTION_DONTCARE;
    bindDesc.mDepthStencil.mStoreAction = STORE_ACTION_DONTCARE;
    cmdBindRenderTargets(cmd_, &bindDesc);
    cmdSetViewport(cmd_, 0.0f, 0.0f, static_cast<float>(targetWidth),
                   static_cast<float>(targetHeight), 0.0f, 1.0f);
    cmdSetScissor(cmd_, 0, 0, static_cast<uint32_t>(targetWidth), static_cast<uint32_t>(targetHeight));

    Pipeline* pipeline = useSwapchain ? meshPipeline_ : meshPipelineOffscreen_;
    if (!pipeline || !meshDescriptorSet_ || !meshUniformBuffer_) {
        return;
    }
    const glm::mat4 viewProj = getViewProjectionMatrix();
    for (const auto& [id, entity] : entities) {
        if (entity.layer != layer || !entity.visible) {
            continue;
        }

        const glm::mat4 translate = glm::translate(glm::mat4(1.0f), entity.position);
        const glm::mat4 rotate = glm::mat4_cast(entity.rotation);
        const glm::mat4 scale = glm::scale(glm::mat4(1.0f), entity.scale);
        const glm::mat4 world = translate * rotate * scale;
        const glm::mat4 mvp = viewProj * world;

        MeshConstants constants{};
        std::memcpy(constants.mvp, glm::value_ptr(mvp), sizeof(constants.mvp));
        graphics::MaterialDesc desc;
        auto matIt = materials.find(entity.material);
        if (matIt != materials.end()) {
            desc = matIt->second;
        }
        constants.color[0] = desc.baseColor.x;
        constants.color[1] = desc.baseColor.y;
        constants.color[2] = desc.baseColor.z;
        constants.color[3] = desc.baseColor.w;

        BufferUpdateDesc ubUpdate = { meshUniformBuffer_ };
        beginUpdateResource(&ubUpdate);
        std::memcpy(ubUpdate.pMappedData, &constants, sizeof(constants));
        endUpdateResource(&ubUpdate);

        auto drawMesh = [&](graphics::MeshId meshId) {
            if (meshId == graphics::kInvalidMesh) {
                return;
            }
            auto meshIt = meshes.find(meshId);
            if (meshIt == meshes.end()) {
                return;
            }
            const MeshRecord& mesh = meshIt->second;
            if (!mesh.vertexBuffer || !mesh.indexBuffer || mesh.indexCount == 0) {
                return;
            }

            Texture* texture = mesh.texture ? mesh.texture : whiteTexture_;
            DescriptorData params[3] = {};
            params[0].mIndex = 0;
            params[0].ppBuffers = &meshUniformBuffer_;
            params[1].mIndex = 1;
            params[1].ppTextures = &texture;
            params[2].mIndex = 2;
            params[2].ppSamplers = &meshSampler_;
            updateDescriptorSet(renderer_, 0, meshDescriptorSet_, 3, params);

            cmdBindPipeline(cmd_, pipeline);
            cmdBindDescriptorSet(cmd_, 0, meshDescriptorSet_);
            uint32_t stride = sizeof(MeshVertex);
            uint64_t offset = 0;
            Buffer* vb = mesh.vertexBuffer;
            cmdBindVertexBuffer(cmd_, 1, &vb, &stride, &offset);
            cmdBindIndexBuffer(cmd_, mesh.indexBuffer, INDEX_TYPE_UINT32, 0);
            cmdDrawIndexed(cmd_, mesh.indexCount, 0, 0);
        };

        if (!entity.meshes.empty()) {
            for (const auto meshId : entity.meshes) {
                drawMesh(meshId);
            }
        } else {
            drawMesh(entity.mesh);
        }
    }

    if (!useSwapchain && renderTarget) {
        RenderTargetBarrier rtEnd{};
        rtEnd.pRenderTarget = renderTarget;
        rtEnd.mCurrentState = RESOURCE_STATE_RENDER_TARGET;
        rtEnd.mNewState = RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        cmdResourceBarrier(cmd_, 0, nullptr, 0, nullptr, 1, &rtEnd);
    }
}

unsigned int ForgeBackend::getRenderTargetTextureId(graphics::RenderTargetId target) const {
    auto it = renderTargets.find(target);
    if (it == renderTargets.end()) {
        return 0u;
    }
    return static_cast<unsigned int>(it->second.token);
}

void ForgeBackend::setUiOverlayTexture(const graphics::TextureHandle& texture) {
    uiOverlayTexture_ = texture;
}

void ForgeBackend::setUiOverlayVisible(bool visible) {
    uiOverlayVisible_ = visible;
}

void ForgeBackend::renderUiOverlay() {
    if (!uiOverlayVisible_ || !uiOverlayTexture_.valid()) {
        static bool loggedOnce = false;
        if (!loggedOnce) {
            spdlog::info("Graphics(Forge): UI overlay skipped visible={} valid={}",
                         uiOverlayVisible_ ? "yes" : "no",
                         uiOverlayTexture_.valid() ? "yes" : "no");
            loggedOnce = true;
        }
        return;
    }
    if (!cmd_ || !renderer_ || !swapChain_) {
        return;
    }
    ensureUiOverlayResources();
    if (!uiOverlayPipeline_ || !uiOverlayDescriptorSet_ || !uiOverlayVertexBuffer_ || !uiOverlayIndexBuffer_ || !uiOverlayUniformBuffer_) {
        return;
    }
    Texture* texture = static_cast<Texture*>(graphics_backend::forge_ui::ResolveExternalTexture(uiOverlayTexture_.id));
    if (!texture) {
        return;
    }

    RenderTarget* backBuffer = swapChain_->ppRenderTargets[frameIndex_];
    BindRenderTargetsDesc bindDesc{};
    bindDesc.mRenderTargetCount = 1;
    bindDesc.mRenderTargets[0].pRenderTarget = backBuffer;
    bindDesc.mRenderTargets[0].mLoadAction = LOAD_ACTION_LOAD;
    bindDesc.mRenderTargets[0].mStoreAction = STORE_ACTION_STORE;
    bindDesc.mDepthStencil.pDepthStencil = nullptr;
    bindDesc.mDepthStencil.mLoadAction = LOAD_ACTION_DONTCARE;
    bindDesc.mDepthStencil.mStoreAction = STORE_ACTION_DONTCARE;
    cmdBindRenderTargets(cmd_, &bindDesc);

    const uint32_t width = framebufferWidth > 0 ? static_cast<uint32_t>(framebufferWidth) : 1u;
    const uint32_t height = framebufferHeight > 0 ? static_cast<uint32_t>(framebufferHeight) : 1u;

    struct UiOverlayConstants {
        float scaleBias[4];
    } constants{};
    constants.scaleBias[0] = 2.0f / static_cast<float>(width);
    constants.scaleBias[1] = -2.0f / static_cast<float>(height);
    constants.scaleBias[2] = -1.0f;
    constants.scaleBias[3] = 1.0f;
    BufferUpdateDesc cbUpdate = { uiOverlayUniformBuffer_ };
    beginUpdateResource(&cbUpdate);
    std::memcpy(cbUpdate.pMappedData, &constants, sizeof(constants));
    endUpdateResource(&cbUpdate);

    struct UiVertex {
        float x;
        float y;
        float u;
        float v;
        uint32_t color;
    };
    const uint32_t color = 0xffffffffu;
    UiVertex vertices[4] = {
        {0.0f, 0.0f, 0.0f, 0.0f, color},
        {static_cast<float>(width), 0.0f, 1.0f, 0.0f, color},
        {static_cast<float>(width), static_cast<float>(height), 1.0f, 1.0f, color},
        {0.0f, static_cast<float>(height), 0.0f, 1.0f, color},
    };
    const uint16_t indices[6] = {0, 1, 2, 0, 2, 3};

    BufferUpdateDesc vbUpdate = { uiOverlayVertexBuffer_ };
    beginUpdateResource(&vbUpdate);
    std::memcpy(vbUpdate.pMappedData, vertices, sizeof(vertices));
    endUpdateResource(&vbUpdate);

    BufferUpdateDesc ibUpdate = { uiOverlayIndexBuffer_ };
    beginUpdateResource(&ibUpdate);
    std::memcpy(ibUpdate.pMappedData, indices, sizeof(indices));
    endUpdateResource(&ibUpdate);

    DescriptorData params[3] = {};
    params[0].mIndex = 0;
    params[0].ppBuffers = &uiOverlayUniformBuffer_;
    params[1].mIndex = 1;
    params[1].ppTextures = &texture;
    params[2].mIndex = 2;
    params[2].ppSamplers = &uiOverlaySampler_;
    updateDescriptorSet(renderer_, 0, uiOverlayDescriptorSet_, 3, params);

    cmdSetViewport(cmd_, 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f);
    cmdSetScissor(cmd_, 0, 0, width, height);
    cmdBindPipeline(cmd_, uiOverlayPipeline_);
    cmdBindDescriptorSet(cmd_, 0, uiOverlayDescriptorSet_);

    uint32_t stride = sizeof(UiVertex);
    uint64_t offset = 0;
    cmdBindVertexBuffer(cmd_, 1, &uiOverlayVertexBuffer_, &stride, &offset);
    cmdBindIndexBuffer(cmd_, uiOverlayIndexBuffer_, INDEX_TYPE_UINT16, 0);
    cmdDrawIndexed(cmd_, 6, 0, 0);
}

void ForgeBackend::setPosition(graphics::EntityId entity, const glm::vec3& position) {
    auto it = entities.find(entity);
    if (it != entities.end()) {
        it->second.position = position;
    }
}

void ForgeBackend::setRotation(graphics::EntityId entity, const glm::quat& rotation) {
    auto it = entities.find(entity);
    if (it != entities.end()) {
        it->second.rotation = rotation;
    }
}

void ForgeBackend::setScale(graphics::EntityId entity, const glm::vec3& scale) {
    auto it = entities.find(entity);
    if (it != entities.end()) {
        it->second.scale = scale;
    }
}

void ForgeBackend::setVisible(graphics::EntityId entity, bool visible) {
    auto it = entities.find(entity);
    if (it != entities.end()) {
        it->second.visible = visible;
    }
}

void ForgeBackend::setTransparency(graphics::EntityId entity, bool transparency) {
    auto it = entities.find(entity);
    if (it != entities.end()) {
        it->second.transparent = transparency;
    }
}

void ForgeBackend::setCameraPosition(const glm::vec3& position) {
    cameraPosition = position;
}

void ForgeBackend::setCameraRotation(const glm::quat& rotation) {
    cameraRotation = rotation;
}

void ForgeBackend::setPerspective(float fovDegreesIn, float aspect, float nearPlane, float farPlaneIn) {
    usePerspective = true;
    fovDegrees = fovDegreesIn;
    aspectRatio = aspect;
    nearPlane = nearPlane;
    farPlane = farPlaneIn;
}

void ForgeBackend::setOrthographic(float left, float right, float top, float bottom,
                                   float nearPlaneIn, float farPlaneIn) {
    usePerspective = false;
    orthoLeft = left;
    orthoRight = right;
    orthoTop = top;
    orthoBottom = bottom;
    nearPlane = nearPlaneIn;
    farPlane = farPlaneIn;
}

glm::mat4 ForgeBackend::getViewProjectionMatrix() const {
    return computeProjectionMatrix() * computeViewMatrix();
}

glm::mat4 ForgeBackend::getViewMatrix() const {
    return computeViewMatrix();
}

glm::mat4 ForgeBackend::getProjectionMatrix() const {
    return computeProjectionMatrix();
}

glm::vec3 ForgeBackend::getCameraPosition() const {
    return cameraPosition;
}

glm::vec3 ForgeBackend::getCameraForward() const {
    return cameraRotation * glm::vec3(0.0f, 0.0f, -1.0f);
}

glm::mat4 ForgeBackend::computeViewMatrix() const {
    const glm::mat4 rotation = glm::mat4_cast(glm::conjugate(cameraRotation));
    const glm::mat4 translation = glm::translate(glm::mat4(1.0f), -cameraPosition);
    return rotation * translation;
}

glm::mat4 ForgeBackend::computeProjectionMatrix() const {
    if (usePerspective) {
        return glm::perspective(glm::radians(fovDegrees), aspectRatio, nearPlane, farPlane);
    }
    return glm::ortho(orthoLeft, orthoRight, orthoBottom, orthoTop, nearPlane, farPlane);
}

void ForgeBackend::ensureUiOverlayResources() {
    if (!renderer_) {
        return;
    }
    if (uiOverlayPipeline_ && uiOverlayDescriptorSet_ && uiOverlayVertexBuffer_ && uiOverlayIndexBuffer_ && uiOverlayUniformBuffer_) {
        return;
    }

    const std::filesystem::path shaderDir = bz::data::Resolve("forge/shaders");
    const auto vsPath = shaderDir / "ui_overlay.vert.spv";
    const auto fsPath = shaderDir / "ui_overlay.frag.spv";
    auto vsBytes = readFileBytes(vsPath);
    auto fsBytes = readFileBytes(fsPath);
    if (vsBytes.empty() || fsBytes.empty()) {
        spdlog::error("Graphics(Forge): missing UI overlay shaders '{}', '{}'", vsPath.string(), fsPath.string());
        return;
    }

    BinaryShaderDesc shaderDesc{};
    shaderDesc.mStages = SHADER_STAGE_VERT | SHADER_STAGE_FRAG;
    shaderDesc.mOwnByteCode = false;
    shaderDesc.mVert = { "ui_overlay.vert", vsBytes.data(), static_cast<uint32_t>(vsBytes.size()), "main" };
    shaderDesc.mFrag = { "ui_overlay.frag", fsBytes.data(), static_cast<uint32_t>(fsBytes.size()), "main" };
    addShaderBinary(renderer_, &shaderDesc, &uiOverlayShader_);
    if (!uiOverlayShader_) {
        spdlog::error("Graphics(Forge): failed to create UI overlay shader");
        return;
    }

    SamplerDesc samplerDesc{};
    samplerDesc.mMinFilter = FILTER_LINEAR;
    samplerDesc.mMagFilter = FILTER_LINEAR;
    samplerDesc.mMipMapMode = MIPMAP_MODE_LINEAR;
    samplerDesc.mAddressU = ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerDesc.mAddressV = ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerDesc.mAddressW = ADDRESS_MODE_CLAMP_TO_EDGE;
    addSampler(renderer_, &samplerDesc, &uiOverlaySampler_);

    uiOverlayDescriptors_[0].mType = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uiOverlayDescriptors_[0].mCount = 1;
    uiOverlayDescriptors_[0].mOffset = 0;
    uiOverlayDescriptors_[1].mType = DESCRIPTOR_TYPE_TEXTURE;
    uiOverlayDescriptors_[1].mCount = 1;
    uiOverlayDescriptors_[1].mOffset = 1;
    uiOverlayDescriptors_[2].mType = DESCRIPTOR_TYPE_SAMPLER;
    uiOverlayDescriptors_[2].mCount = 1;
    uiOverlayDescriptors_[2].mOffset = 2;

    DescriptorSetDesc setDesc{};
    setDesc.mIndex = 0;
    setDesc.mMaxSets = 1;
    setDesc.mDescriptorCount = 3;
    setDesc.pDescriptors = uiOverlayDescriptors_;
    addDescriptorSet(renderer_, &setDesc, &uiOverlayDescriptorSet_);

    BufferLoadDesc vbDesc{};
    vbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
    vbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
    vbDesc.mDesc.mSize = sizeof(float) * 5 * 4;
    vbDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
    vbDesc.mDesc.pName = "Forge UI Overlay VB";
    vbDesc.ppBuffer = &uiOverlayVertexBuffer_;
    addResource(&vbDesc, nullptr);

    BufferLoadDesc ibDesc = vbDesc;
    ibDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_INDEX_BUFFER;
    ibDesc.mDesc.mSize = sizeof(uint16_t) * 6;
    ibDesc.mDesc.pName = "Forge UI Overlay IB";
    ibDesc.ppBuffer = &uiOverlayIndexBuffer_;
    addResource(&ibDesc, nullptr);

    BufferLoadDesc ubDesc{};
    ubDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    ubDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
    ubDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
    ubDesc.mDesc.mSize = sizeof(float) * 4;
    ubDesc.mDesc.pName = "Forge UI Overlay UB";
    ubDesc.ppBuffer = &uiOverlayUniformBuffer_;
    addResource(&ubDesc, nullptr);

    VertexLayout layout{};
    layout.mBindingCount = 1;
    layout.mAttribCount = 3;
    layout.mBindings[0].mStride = sizeof(float) * 5;
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

    const TinyImageFormat colorFormat = swapChain_ ? swapChain_->ppRenderTargets[0]->mFormat : TinyImageFormat_R8G8B8A8_UNORM;
    PipelineDesc pipelineDesc{};
    pipelineDesc.mType = PIPELINE_TYPE_GRAPHICS;
    pipelineDesc.mGraphicsDesc.pShaderProgram = uiOverlayShader_;
    pipelineDesc.mGraphicsDesc.pVertexLayout = &layout;
    pipelineDesc.mGraphicsDesc.pBlendState = &blend;
    pipelineDesc.mGraphicsDesc.pDepthState = &depth;
    pipelineDesc.mGraphicsDesc.pRasterizerState = &raster;
    pipelineDesc.mGraphicsDesc.mRenderTargetCount = 1;
    pipelineDesc.mGraphicsDesc.mSampleCount = SAMPLE_COUNT_1;
    pipelineDesc.mGraphicsDesc.mSampleQuality = 0;
    pipelineDesc.mGraphicsDesc.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
    pipelineDesc.mGraphicsDesc.pColorFormats = const_cast<TinyImageFormat*>(&colorFormat);
    pipelineDesc.mGraphicsDesc.mDepthStencilFormat = TinyImageFormat_UNDEFINED;

    DescriptorSetLayoutDesc layoutDesc{};
    layoutDesc.pDescriptors = uiOverlayDescriptors_;
    layoutDesc.mDescriptorCount = 3;
    layoutDesc.pStaticSamplers = nullptr;
    layoutDesc.mStaticSamplerCount = 0;
    const DescriptorSetLayoutDesc* layoutPtrs[] = { &layoutDesc };
    pipelineDesc.pLayouts = layoutPtrs;
    pipelineDesc.mLayoutCount = 1;

    addPipeline(renderer_, &pipelineDesc, &uiOverlayPipeline_);
}

void ForgeBackend::destroyUiOverlayResources() {
    if (!renderer_) {
        return;
    }
    if (uiOverlayPipeline_) {
        removePipeline(renderer_, uiOverlayPipeline_);
        uiOverlayPipeline_ = nullptr;
    }
    if (uiOverlayShader_) {
        removeShader(renderer_, uiOverlayShader_);
        uiOverlayShader_ = nullptr;
    }
    if (uiOverlayDescriptorSet_) {
        removeDescriptorSet(renderer_, uiOverlayDescriptorSet_);
        uiOverlayDescriptorSet_ = nullptr;
    }
    if (uiOverlaySampler_) {
        removeSampler(renderer_, uiOverlaySampler_);
        uiOverlaySampler_ = nullptr;
    }
    if (uiOverlayVertexBuffer_) {
        removeResource(uiOverlayVertexBuffer_);
        uiOverlayVertexBuffer_ = nullptr;
    }
    if (uiOverlayIndexBuffer_) {
        removeResource(uiOverlayIndexBuffer_);
        uiOverlayIndexBuffer_ = nullptr;
    }
    if (uiOverlayUniformBuffer_) {
        removeResource(uiOverlayUniformBuffer_);
        uiOverlayUniformBuffer_ = nullptr;
    }
}

void ForgeBackend::ensureMeshResources() {
    if (!renderer_) {
        return;
    }
    if (meshPipeline_ && meshPipelineOffscreen_ && meshDescriptorSet_ && meshUniformBuffer_ && meshSampler_) {
        return;
    }

    const std::filesystem::path shaderDir = bz::data::Resolve("forge/shaders");
    const auto vsPath = shaderDir / "mesh.vert.spv";
    const auto fsPath = shaderDir / "mesh.frag.spv";
    auto vsBytes = readFileBytes(vsPath);
    auto fsBytes = readFileBytes(fsPath);
    if (vsBytes.empty() || fsBytes.empty()) {
        spdlog::error("Graphics(Forge): missing mesh shaders '{}', '{}'", vsPath.string(), fsPath.string());
        return;
    }

    if (!meshShader_) {
        BinaryShaderDesc shaderDesc{};
        shaderDesc.mStages = SHADER_STAGE_VERT | SHADER_STAGE_FRAG;
        shaderDesc.mOwnByteCode = false;
        shaderDesc.mVert = {"mesh.vert", vsBytes.data(), static_cast<uint32_t>(vsBytes.size()), "main"};
        shaderDesc.mFrag = {"mesh.frag", fsBytes.data(), static_cast<uint32_t>(fsBytes.size()), "main"};
        addShaderBinary(renderer_, &shaderDesc, &meshShader_);
        if (!meshShader_) {
            spdlog::error("Graphics(Forge): failed to create mesh shader");
            return;
        }
    }

    if (!meshSampler_) {
        SamplerDesc samplerDesc{};
        samplerDesc.mMinFilter = FILTER_LINEAR;
        samplerDesc.mMagFilter = FILTER_LINEAR;
        samplerDesc.mMipMapMode = MIPMAP_MODE_LINEAR;
        samplerDesc.mAddressU = ADDRESS_MODE_REPEAT;
        samplerDesc.mAddressV = ADDRESS_MODE_REPEAT;
        samplerDesc.mAddressW = ADDRESS_MODE_REPEAT;
        addSampler(renderer_, &samplerDesc, &meshSampler_);
    }

    meshDescriptors_[0].mType = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    meshDescriptors_[0].mCount = 1;
    meshDescriptors_[0].mOffset = 0;
    meshDescriptors_[1].mType = DESCRIPTOR_TYPE_TEXTURE;
    meshDescriptors_[1].mCount = 1;
    meshDescriptors_[1].mOffset = 1;
    meshDescriptors_[2].mType = DESCRIPTOR_TYPE_SAMPLER;
    meshDescriptors_[2].mCount = 1;
    meshDescriptors_[2].mOffset = 2;

    if (!meshDescriptorSet_) {
        DescriptorSetDesc setDesc{};
        setDesc.mIndex = 0;
        setDesc.mMaxSets = 1;
        setDesc.mDescriptorCount = 3;
        setDesc.pDescriptors = meshDescriptors_;
        addDescriptorSet(renderer_, &setDesc, &meshDescriptorSet_);
    }

    if (!meshUniformBuffer_) {
        BufferLoadDesc ubDesc{};
        ubDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        ubDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
        ubDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
        ubDesc.mDesc.mSize = sizeof(MeshConstants);
        ubDesc.mDesc.pName = "Forge Mesh UB";
        ubDesc.ppBuffer = &meshUniformBuffer_;
        addResource(&ubDesc, nullptr);
    }

    if (!whiteTexture_) {
        TextureDesc texDesc{};
        texDesc.mWidth = 1;
        texDesc.mHeight = 1;
        texDesc.mDepth = 1;
        texDesc.mMipLevels = 1;
        texDesc.mArraySize = 1;
        texDesc.mFormat = TinyImageFormat_R8G8B8A8_UNORM;
        texDesc.mSampleCount = SAMPLE_COUNT_1;
        texDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
        texDesc.mStartState = RESOURCE_STATE_COPY_DEST;
        texDesc.pName = "Forge White Texture";
        TextureLoadDesc loadDesc{};
        loadDesc.pDesc = &texDesc;
        loadDesc.ppTexture = &whiteTexture_;
        SyncToken token{};
        addResource(&loadDesc, &token);
        waitForToken(&token);

        const uint32_t whitePixel = 0xffffffffu;
        TextureUpdateDesc updateDesc = { whiteTexture_, 0, 1, 0, 1, RESOURCE_STATE_PIXEL_SHADER_RESOURCE };
        beginUpdateResource(&updateDesc);
        TextureSubresourceUpdate subresource = updateDesc.getSubresourceUpdateDesc(0, 0);
        for (uint32_t row = 0; row < subresource.mRowCount; ++row) {
            std::memcpy(subresource.pMappedData + row * subresource.mDstRowStride,
                        reinterpret_cast<const uint8_t*>(&whitePixel) + row * subresource.mSrcRowStride,
                        subresource.mSrcRowStride);
        }
        endUpdateResource(&updateDesc);

        if (renderFence_) {
            FlushResourceUpdateDesc flush{};
            flush.pOutFence = renderFence_;
            flushResourceUpdates(&flush);
            waitForFences(renderer_, 1, &renderFence_);
        } else {
            waitForAllResourceLoads();
        }
    }

    VertexLayout layout{};
    layout.mBindingCount = 1;
    layout.mAttribCount = 3;
    layout.mBindings[0].mStride = sizeof(MeshVertex);
    layout.mBindings[0].mRate = VERTEX_BINDING_RATE_VERTEX;
    layout.mAttribs[0].mSemantic = SEMANTIC_POSITION;
    layout.mAttribs[0].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
    layout.mAttribs[0].mBinding = 0;
    layout.mAttribs[0].mLocation = 0;
    layout.mAttribs[0].mOffset = 0;
    layout.mAttribs[1].mSemantic = SEMANTIC_NORMAL;
    layout.mAttribs[1].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
    layout.mAttribs[1].mBinding = 0;
    layout.mAttribs[1].mLocation = 1;
    layout.mAttribs[1].mOffset = sizeof(float) * 3;
    layout.mAttribs[2].mSemantic = SEMANTIC_TEXCOORD0;
    layout.mAttribs[2].mFormat = TinyImageFormat_R32G32_SFLOAT;
    layout.mAttribs[2].mBinding = 0;
    layout.mAttribs[2].mLocation = 2;
    layout.mAttribs[2].mOffset = sizeof(float) * 6;

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
    raster.mScissor = false;

    DescriptorSetLayoutDesc layoutDesc{};
    layoutDesc.pDescriptors = meshDescriptors_;
    layoutDesc.mDescriptorCount = 3;
    const DescriptorSetLayoutDesc* layoutPtrs[] = { &layoutDesc };

    auto buildPipeline = [&](TinyImageFormat colorFormat, Pipeline** pipeline) {
        if (*pipeline) {
            return;
        }
        PipelineDesc pipelineDesc{};
        pipelineDesc.mType = PIPELINE_TYPE_GRAPHICS;
        pipelineDesc.mGraphicsDesc.pShaderProgram = meshShader_;
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
        pipelineDesc.pLayouts = layoutPtrs;
        pipelineDesc.mLayoutCount = 1;
        addPipeline(renderer_, &pipelineDesc, pipeline);
    };

    const TinyImageFormat swapFormat = swapChain_ ? swapChain_->ppRenderTargets[0]->mFormat : TinyImageFormat_R8G8B8A8_UNORM;
    buildPipeline(swapFormat, &meshPipeline_);
    buildPipeline(TinyImageFormat_R8G8B8A8_UNORM, &meshPipelineOffscreen_);
}

void ForgeBackend::destroyMeshResources() {
    if (!renderer_) {
        return;
    }
    if (meshPipeline_) {
        removePipeline(renderer_, meshPipeline_);
        meshPipeline_ = nullptr;
    }
    if (meshPipelineOffscreen_) {
        removePipeline(renderer_, meshPipelineOffscreen_);
        meshPipelineOffscreen_ = nullptr;
    }
    if (meshShader_) {
        removeShader(renderer_, meshShader_);
        meshShader_ = nullptr;
    }
    if (meshDescriptorSet_) {
        removeDescriptorSet(renderer_, meshDescriptorSet_);
        meshDescriptorSet_ = nullptr;
    }
    if (meshUniformBuffer_) {
        removeResource(meshUniformBuffer_);
        meshUniformBuffer_ = nullptr;
    }
    if (whiteTexture_) {
        removeResource(whiteTexture_);
        whiteTexture_ = nullptr;
    }
    if (meshSampler_) {
        removeSampler(renderer_, meshSampler_);
        meshSampler_ = nullptr;
    }
}

} // namespace graphics_backend
