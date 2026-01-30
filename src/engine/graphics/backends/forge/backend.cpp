#include "engine/graphics/backends/forge/backend.hpp"

#include "engine/graphics/backends/forge/ui_bridge.hpp"
#include "engine/geometry/mesh_loader.hpp"
#include "common/data_path_resolver.hpp"
#include "common/file_utils.hpp"
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
#include <cmath>
#include <cstdlib>
#include <limits>
#include <unordered_set>

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

struct MeshVertex {
    float px, py, pz;
    float nx, ny, nz;
    float u, v;
};

struct MeshConstants {
    float mvp[16];
    float color[4];
};

constexpr uint32_t kDescriptorSetRingSize = 3;

} // namespace

ForgeBackend::ForgeBackend(platform::Window& windowRef) : window(&windowRef) {
    spdlog::warn("Graphics(Forge): init begin");
    int fbWidth = 0;
    int fbHeight = 0;
    window->getFramebufferSize(fbWidth, fbHeight);
    framebufferWidth = fbWidth;
    framebufferHeight = fbHeight;

    initMemAlloc("bz3");
    spdlog::warn("Graphics(Forge): initMemAlloc ok");
    ensureForgeConfigFiles();
    FileSystemInitDesc fsDesc{};
    fsDesc.pAppName = "bz3";
    if (!initFileSystem(&fsDesc)) {
        spdlog::error("Graphics(Forge): initFileSystem failed");
        return;
    }
    initLog("bz3", eALL);
    spdlog::warn("Graphics(Forge): initFileSystem/initLog ok");
    initGPUConfiguration(nullptr);
    spdlog::warn("Graphics(Forge): initGPUConfiguration ok");

    RendererDesc rendererDesc{};
    rendererDesc.mGpuMode = GPU_MODE_SINGLE;
    rendererDesc.mShaderTarget = SHADER_TARGET_6_0;
#if defined(VULKAN)
    static const char* kValidationLayer = "VK_LAYER_KHRONOS_validation";
    const bool enableValidation = []() {
        const char* flag = std::getenv("BZ3_FORGE_ENABLE_VALIDATION");
        return flag && flag[0] == '1';
    }();
    if (enableValidation) {
        rendererDesc.mVk.ppInstanceLayers = &kValidationLayer;
        rendererDesc.mVk.mInstanceLayerCount = 1;
        spdlog::warn("Graphics(Forge): Vulkan validation enabled (layer {})", kValidationLayer);
    }
#endif
    initRenderer("bz3", &rendererDesc, &renderer_);
    if (!renderer_) {
        spdlog::error("Graphics(Forge): failed to initialize renderer.");
        return;
    }
    spdlog::warn("Graphics(Forge): initRenderer ok");
    initResourceLoaderInterface(renderer_, nullptr);
    setupGPUConfigurationPlatformParameters(renderer_, nullptr);
    spdlog::warn("Graphics(Forge): setupGPUConfigurationPlatformParameters ok");

    QueueDesc queueDesc{};
    queueDesc.mType = QUEUE_TYPE_GRAPHICS;
    queueDesc.mPriority = QUEUE_PRIORITY_NORMAL;
    initQueue(renderer_, &queueDesc, &graphicsQueue_);
    if (!graphicsQueue_) {
        spdlog::error("Graphics(Forge): failed to create graphics queue.");
        return;
    }
    spdlog::warn("Graphics(Forge): initQueue ok");

    WindowHandle handle = buildWindowHandle(window);
    if (handle.type == WINDOW_HANDLE_TYPE_UNKNOWN) {
        spdlog::error("Graphics(Forge): unsupported SDL3 native window handle.");
        return;
    }
    spdlog::warn("Graphics(Forge): buildWindowHandle ok type={}", static_cast<int>(handle.type));

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
    spdlog::warn("Graphics(Forge): addSwapChain ok");

    initFence(renderer_, &renderFence_);
    initSemaphore(renderer_, &imageAcquiredSemaphore_);
    initSemaphore(renderer_, &renderCompleteSemaphore_);
    CmdPoolDesc cmdPoolDesc{};
    cmdPoolDesc.pQueue = graphicsQueue_;
    initCmdPool(renderer_, &cmdPoolDesc, &cmdPool_);
    CmdDesc cmdDesc{};
    cmdDesc.pPool = cmdPool_;
    initCmd(renderer_, &cmdDesc, &cmd_);
    spdlog::warn("Graphics(Forge): sync primitives ok");

    const uint32_t colorFormat = swapChain_->ppRenderTargets[0]->mFormat;
    graphics_backend::forge_ui::SetContext(renderer_, graphicsQueue_, framebufferWidth, framebufferHeight, colorFormat);
    spdlog::warn("Graphics(Forge): ui bridge context ok");

#if defined(BZ3_UI_BACKEND_IMGUI)
    uiBridge_ = std::make_unique<ForgeRenderer>();
#endif
    spdlog::warn("Graphics(Forge): initialized (SDL3 + Vulkan).");
}

ForgeBackend::~ForgeBackend() {
    if (graphicsQueue_) {
        waitQueueIdle(graphicsQueue_);
    }
    uiBridge_.reset();
    destroyUiOverlayResources();
    destroyBrightnessResources();
    destroySceneTarget();
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
    acquireNextImage(renderer_, swapChain_, imageAcquiredSemaphore_, nullptr, &frameIndex_);
    if (!cmd_) {
        return;
    }
    beginCmd(cmd_);
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
    destroySceneTarget();
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
    static bool loggedMain = false;
    if (!loggedMain && layer == 0) {
        spdlog::warn("Graphics(Forge): createEntity main layer id={}", static_cast<unsigned int>(id));
        loggedMain = true;
    }
    return id;
}

graphics::EntityId ForgeBackend::createModelEntity(const std::filesystem::path& modelPath,
                                                   graphics::LayerId layer,
                                                   graphics::MaterialId materialOverride) {
    const auto id = createEntity(layer);
    static bool loggedMainModel = false;
    if (!loggedMainModel && layer == 0) {
        spdlog::warn("Graphics(Forge): createModelEntity main layer id={} model='{}'",
                     static_cast<unsigned int>(id), modelPath.string());
        loggedMainModel = true;
    }
    setEntityModel(id, modelPath, materialOverride);
    return id;
}

graphics::EntityId ForgeBackend::createMeshEntity(graphics::MeshId mesh,
                                                  graphics::LayerId layer,
                                                  graphics::MaterialId materialOverride) {
    const auto id = createEntity(layer);
    static bool loggedMainMesh = false;
    if (!loggedMainMesh && layer == 0) {
        spdlog::warn("Graphics(Forge): createMeshEntity main layer id={} mesh={}",
                     static_cast<unsigned int>(id), static_cast<unsigned int>(mesh));
        loggedMainMesh = true;
    }
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
    static bool loggedMainSet = false;
    if (!loggedMainSet && it->second.layer == 0) {
        spdlog::warn("Graphics(Forge): setEntityModel main layer id={} model='{}'",
                     static_cast<unsigned int>(entity), modelPath.string());
        loggedMainSet = true;
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
        static bool loggedEmpty = false;
        if (!loggedEmpty && it->second.layer == 0) {
            spdlog::warn("Graphics(Forge): setEntityModel main layer loadGLB empty path='{}'",
                         resolved.string());
            loggedEmpty = true;
        }
        return;
    }
    static bool loggedLoaded = false;
    if (!loggedLoaded && it->second.layer == 0) {
        spdlog::warn("Graphics(Forge): setEntityModel main layer loaded meshes={} path='{}'",
                     loaded.size(), resolved.string());
        loggedLoaded = true;
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
    static bool loggedMeshes = false;
    if (!loggedMeshes && it->second.layer == 0) {
        spdlog::warn("Graphics(Forge): setEntityModel main layer meshes={} firstMesh={}",
                     it->second.meshes.size(),
                     static_cast<unsigned int>(it->second.mesh));
        loggedMeshes = true;
    }
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
    if (const char* dbg = std::getenv("BZ3_FORGE_DEBUG_MESH_BOUNDS"); dbg && dbg[0] == '1') {
        glm::vec3 minv{std::numeric_limits<float>::max()};
        glm::vec3 maxv{std::numeric_limits<float>::lowest()};
        for (const auto& v : mesh.vertices) {
            minv = glm::min(minv, v);
            maxv = glm::max(maxv, v);
        }
        spdlog::warn("Graphics(Forge): mesh bounds min=({}, {}, {}) max=({}, {}, {}) verts={} indices={}",
                     minv.x, minv.y, minv.z, maxv.x, maxv.y, maxv.z,
                     mesh.vertices.size(), mesh.indices.size());
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
    static bool loggedRenderLayerDefault = false;
    static bool loggedRenderLayerOther = false;
    if (target == graphics::kDefaultRenderTarget) {
        if (!loggedRenderLayerDefault) {
            spdlog::warn("Graphics(Forge): renderLayer begin layer={} target=default fb={}x{}",
                         static_cast<int>(layer), framebufferWidth, framebufferHeight);
            loggedRenderLayerDefault = true;
        }
    } else if (!loggedRenderLayerOther) {
        spdlog::warn("Graphics(Forge): renderLayer begin layer={} target={} fb={}x{}",
                     static_cast<int>(layer), static_cast<unsigned int>(target),
                     framebufferWidth, framebufferHeight);
        loggedRenderLayerOther = true;
    }

    RenderTarget* renderTarget = nullptr;
    int targetWidth = framebufferWidth;
    int targetHeight = framebufferHeight;
    bool wantsBrightness = (target == graphics::kDefaultRenderTarget && std::abs(brightness_ - 1.0f) > 0.0001f);
    bool useSwapchain = (target == graphics::kDefaultRenderTarget && !wantsBrightness);
    static bool whiteTextureTransitioned = false;
    if (whiteTexture_ && !whiteTextureTransitioned) {
        TextureBarrier texBarrier{};
        texBarrier.pTexture = whiteTexture_;
        texBarrier.mCurrentState = RESOURCE_STATE_COPY_DEST;
        texBarrier.mNewState = RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        cmdResourceBarrier(cmd_, 0, nullptr, 1, &texBarrier, 0, nullptr);
        whiteTextureTransitioned = true;
        spdlog::warn("Graphics(Forge): transitioned white texture to shader resource");
    }
    if (useSwapchain) {
        if (!swapChain_) {
            return;
        }
        renderTarget = swapChain_->ppRenderTargets[frameIndex_];
    } else if (wantsBrightness) {
        ensureSceneTarget(framebufferWidth, framebufferHeight);
        if (!sceneTarget_) {
            if (!swapChain_) {
                return;
            }
            renderTarget = swapChain_->ppRenderTargets[frameIndex_];
            wantsBrightness = false;
            useSwapchain = true;
        } else {
            renderTarget = sceneTarget_;
        }
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
    const bool debugSwapchainClear = []() {
        const char* flag = std::getenv("BZ3_FORGE_DEBUG_CLEAR_SWAPCHAIN");
        return flag && flag[0] == '1';
    }();
    if ((useSwapchain || wantsBrightness) && debugSwapchainClear) {
        bindDesc.mRenderTargets[0].mClearValue = {1.0f, 0.0f, 1.0f, 1.0f};
        static bool loggedOnce = false;
        if (!loggedOnce) {
            spdlog::warn("Graphics(Forge): debug swapchain clear magenta");
            loggedOnce = true;
        }
    } else if (useSwapchain || wantsBrightness) {
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

    const bool singleDescriptor = []() {
        const char* flag = std::getenv("BZ3_FORGE_DEBUG_SINGLE_DESCRIPTOR");
        return flag && flag[0] == '1';
    }();

    const bool debugUiQuad = []() {
        const char* flag = std::getenv("BZ3_FORGE_DEBUG_UI_QUAD");
        return flag && flag[0] == '1';
    }();
    if (debugUiQuad && target == graphics::kDefaultRenderTarget) {
        ensureUiOverlayResources();
        if (uiOverlayPipeline_ && uiOverlayDescriptorSet_ && uiOverlayVertexBuffer_ &&
            uiOverlayIndexBuffer_ && uiOverlayUniformBuffer_ && whiteTexture_) {
            struct UiOverlayConstants {
                float scaleBias[4];
            } constants{};
            constants.scaleBias[0] = 2.0f / static_cast<float>(targetWidth);
            constants.scaleBias[1] = -2.0f / static_cast<float>(targetHeight);
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
            const uint32_t color = 0xffff00ffu; // magenta
            UiVertex vertices[4] = {
                {0.0f, 0.0f, 0.0f, 0.0f, color},
                {static_cast<float>(targetWidth), 0.0f, 1.0f, 0.0f, color},
                {static_cast<float>(targetWidth), static_cast<float>(targetHeight), 1.0f, 1.0f, color},
                {0.0f, static_cast<float>(targetHeight), 0.0f, 1.0f, color},
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

            Texture* texture = whiteTexture_;
            DescriptorData params[3] = {};
            params[0].mIndex = 0;
            params[0].ppBuffers = &uiOverlayUniformBuffer_;
            params[1].mIndex = 1;
            params[1].ppTextures = &texture;
            params[2].mIndex = 2;
            params[2].ppSamplers = &uiOverlaySampler_;
            updateDescriptorSet(renderer_, setIndex, uiOverlayDescriptorSet_, 3, params);

            cmdBindPipeline(cmd_, uiOverlayPipeline_);
            cmdBindDescriptorSet(cmd_, setIndex, uiOverlayDescriptorSet_);
            uint32_t stride = sizeof(UiVertex);
            uint64_t offset = 0;
            cmdBindVertexBuffer(cmd_, 1, &uiOverlayVertexBuffer_, &stride, &offset);
            cmdBindIndexBuffer(cmd_, uiOverlayIndexBuffer_, INDEX_TYPE_UINT16, 0);
            cmdDrawIndexed(cmd_, 6, 0, 0);

            static bool loggedOnce = false;
            if (!loggedOnce) {
                spdlog::warn("Graphics(Forge): debug UI quad draw issued");
                loggedOnce = true;
            }
        }
    }

    Pipeline* pipeline = useSwapchain ? meshPipeline_ : meshPipelineOffscreen_;
    if (!pipeline || !meshDescriptorSet_ || !meshUniformBuffer_) {
        static bool loggedMissing = false;
        if (!loggedMissing) {
            spdlog::warn("Graphics(Forge): renderLayer skipped (pipeline={} set={} ub={})",
                         pipeline ? "yes" : "no",
                         meshDescriptorSet_ ? "yes" : "no",
                         meshUniformBuffer_ ? "yes" : "no");
            loggedMissing = true;
        }
        return;
    }
    const glm::mat4 viewProj = getViewProjectionMatrix();
    int visibleEntities = 0;
    int meshesDrawn = 0;
    const bool debugCamera = []() {
        const char* flag = std::getenv("BZ3_FORGE_DEBUG_CAMERA");
        return flag && flag[0] == '1';
    }();
    static std::unordered_set<int> loggedLayerCamera;
    if (debugCamera && loggedLayerCamera.insert(static_cast<int>(layer)).second) {
        spdlog::warn("Graphics(Forge): camera pos=({}, {}, {}) rot=({}, {}, {}, {}) fov={} aspect={} near={} far={} persp={}",
                     cameraPosition.x, cameraPosition.y, cameraPosition.z,
                     cameraRotation.w, cameraRotation.x, cameraRotation.y, cameraRotation.z,
                     fovDegrees, aspectRatio, nearPlane, farPlane,
                     usePerspective ? "yes" : "no");
        const float* vp = glm::value_ptr(viewProj);
        spdlog::warn("Graphics(Forge): viewProj [{:.4f} {:.4f} {:.4f} {:.4f}] [{:.4f} {:.4f} {:.4f} {:.4f}] [{:.4f} {:.4f} {:.4f} {:.4f}] [{:.4f} {:.4f} {:.4f} {:.4f}]",
                     vp[0], vp[1], vp[2], vp[3],
                     vp[4], vp[5], vp[6], vp[7],
                     vp[8], vp[9], vp[10], vp[11],
                     vp[12], vp[13], vp[14], vp[15]);
    }
    const bool debugMeshTri = []() {
        const char* flag = std::getenv("BZ3_FORGE_DEBUG_MESH_TRI");
        return flag && flag[0] == '1';
    }();
    const bool debugOnlyTri = []() {
        const char* flag = std::getenv("BZ3_FORGE_DEBUG_ONLY_TRI");
        return flag && flag[0] == '1';
    }();
    if (debugMeshTri && target == graphics::kDefaultRenderTarget) {
        static Buffer* debugVB = nullptr;
        if (!debugVB) {
            MeshVertex triVerts[3] = {
                {-0.5f, -0.5f, 0.0f, 0, 0, 1, 0, 0},
                {0.5f, -0.5f, 0.0f, 0, 0, 1, 1, 0},
                {0.0f, 0.5f, 0.0f, 0, 0, 1, 0.5f, 1},
            };
            BufferLoadDesc vbDesc{};
            vbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
            vbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
            vbDesc.mDesc.mSize = sizeof(triVerts);
            vbDesc.mDesc.pName = "Forge Debug Mesh Tri VB";
            vbDesc.pData = triVerts;
            vbDesc.ppBuffer = &debugVB;
            addResource(&vbDesc, nullptr);
        }

        MeshConstants constants{};
        glm::mat4 identity(1.0f);
        std::memcpy(constants.mvp, glm::value_ptr(identity), sizeof(constants.mvp));
        constants.color[0] = 1.0f;
        constants.color[1] = 0.0f;
        constants.color[2] = 1.0f;
        constants.color[3] = 1.0f;
        BufferUpdateDesc ubUpdate = { meshUniformBuffer_ };
        beginUpdateResource(&ubUpdate);
        std::memcpy(ubUpdate.pMappedData, &constants, sizeof(constants));
        endUpdateResource(&ubUpdate);

        Texture* texture = whiteTexture_;
        if (!singleDescriptor) {
            DescriptorData params[3] = {};
            params[0].mIndex = 0;
            params[0].ppBuffers = &meshUniformBuffer_;
            params[1].mIndex = 1;
            params[1].ppTextures = &texture;
            params[2].mIndex = 2;
            params[2].ppSamplers = &meshSampler_;
            updateDescriptorSet(renderer_, setIndex, meshDescriptorSet_, 3, params);
        }

        cmdBindPipeline(cmd_, pipeline);
        cmdBindDescriptorSet(cmd_, setIndex, meshDescriptorSet_);
        uint32_t stride = sizeof(MeshVertex);
        uint64_t offset = 0;
        cmdBindVertexBuffer(cmd_, 1, &debugVB, &stride, &offset);
        cmdDraw(cmd_, 3, 0);

        static bool loggedTri = false;
        if (!loggedTri) {
            spdlog::warn("Graphics(Forge): debug mesh triangle draw issued");
            loggedTri = true;
        }
    }

    if (debugOnlyTri) {
        return;
    }

    if (singleDescriptor) {
        Texture* texture = whiteTexture_;
        DescriptorData params[3] = {};
        params[0].mIndex = 0;
        params[0].ppBuffers = &meshUniformBuffer_;
        params[1].mIndex = 1;
        params[1].ppTextures = &texture;
        params[2].mIndex = 2;
        params[2].ppSamplers = &meshSampler_;
        updateDescriptorSet(renderer_, setIndex, meshDescriptorSet_, 3, params);
    }

    for (const auto& [id, entity] : entities) {
        if (entity.layer != layer || !entity.visible) {
            continue;
        }
        visibleEntities++;

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
            static bool loggedDraw = false;
            if (!loggedDraw) {
                spdlog::warn("Graphics(Forge): draw mesh id={} indices={} texture={} swapchain={} target={}x{}",
                             static_cast<unsigned int>(meshId), mesh.indexCount,
                             texture ? "yes" : "no",
                             useSwapchain ? "yes" : "no",
                             targetWidth, targetHeight);
                if (debugCamera && loggedLayerCamera.insert(static_cast<int>(layer)).second) {
                    spdlog::warn("Graphics(Forge): camera pos=({}, {}, {}) rot=({}, {}, {}, {}) fov={} aspect={} near={} far={} persp={}",
                                 cameraPosition.x, cameraPosition.y, cameraPosition.z,
                                 cameraRotation.w, cameraRotation.x, cameraRotation.y, cameraRotation.z,
                                 fovDegrees, aspectRatio, nearPlane, farPlane,
                                 usePerspective ? "yes" : "no");
                    const float* vp = glm::value_ptr(viewProj);
                    spdlog::warn("Graphics(Forge): viewProj [{:.4f} {:.4f} {:.4f} {:.4f}] [{:.4f} {:.4f} {:.4f} {:.4f}] [{:.4f} {:.4f} {:.4f} {:.4f}] [{:.4f} {:.4f} {:.4f} {:.4f}]",
                                 vp[0], vp[1], vp[2], vp[3],
                                 vp[4], vp[5], vp[6], vp[7],
                                 vp[8], vp[9], vp[10], vp[11],
                                 vp[12], vp[13], vp[14], vp[15]);
                    const float* wm = glm::value_ptr(world);
                    spdlog::warn("Graphics(Forge): world [{:.4f} {:.4f} {:.4f} {:.4f}] [{:.4f} {:.4f} {:.4f} {:.4f}] [{:.4f} {:.4f} {:.4f} {:.4f}] [{:.4f} {:.4f} {:.4f} {:.4f}]",
                                 wm[0], wm[1], wm[2], wm[3],
                                 wm[4], wm[5], wm[6], wm[7],
                                 wm[8], wm[9], wm[10], wm[11],
                                 wm[12], wm[13], wm[14], wm[15]);
                    spdlog::warn("Graphics(Forge): material color=({}, {}, {}, {})",
                                 constants.color[0], constants.color[1], constants.color[2], constants.color[3]);
                }
                loggedDraw = true;
            }
            if (!singleDescriptor) {
                DescriptorData params[3] = {};
                params[0].mIndex = 0;
                params[0].ppBuffers = &meshUniformBuffer_;
                params[1].mIndex = 1;
                params[1].ppTextures = &texture;
                params[2].mIndex = 2;
                params[2].ppSamplers = &meshSampler_;
                updateDescriptorSet(renderer_, setIndex, meshDescriptorSet_, 3, params);
            }

            cmdBindPipeline(cmd_, pipeline);
            cmdBindDescriptorSet(cmd_, setIndex, meshDescriptorSet_);
            uint32_t stride = sizeof(MeshVertex);
            uint64_t offset = 0;
            Buffer* vb = mesh.vertexBuffer;
            cmdBindVertexBuffer(cmd_, 1, &vb, &stride, &offset);
            cmdBindIndexBuffer(cmd_, mesh.indexBuffer, INDEX_TYPE_UINT32, 0);
            cmdDrawIndexed(cmd_, mesh.indexCount, 0, 0);
            meshesDrawn++;
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
        cmdBindRenderTargets(cmd_, nullptr);
        RenderTargetBarrier rtEnd{};
        rtEnd.pRenderTarget = renderTarget;
        rtEnd.mCurrentState = RESOURCE_STATE_RENDER_TARGET;
        rtEnd.mNewState = RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        cmdResourceBarrier(cmd_, 0, nullptr, 0, nullptr, 1, &rtEnd);
    }

    if (wantsBrightness && sceneTarget_) {
        renderBrightnessPass();
    }
    cmdBindRenderTargets(cmd_, nullptr);
    static bool loggedSummaryDefaultEmpty = false;
    static bool loggedSummaryOther = false;
    if (target == graphics::kDefaultRenderTarget) {
        if ((visibleEntities > 0 || meshesDrawn > 0)) {
            spdlog::warn("Graphics(Forge): renderLayer summary layer={} target=default useSwapchain={} brightness={} entities={} meshes={} size={}x{}",
                         static_cast<int>(layer),
                         useSwapchain ? "yes" : "no",
                         wantsBrightness ? "yes" : "no",
                         visibleEntities, meshesDrawn, targetWidth, targetHeight);
        } else if (!loggedSummaryDefaultEmpty) {
            spdlog::warn("Graphics(Forge): renderLayer summary layer={} target=default useSwapchain={} brightness={} entities={} meshes={} size={}x{}",
                         static_cast<int>(layer),
                         useSwapchain ? "yes" : "no",
                         wantsBrightness ? "yes" : "no",
                         visibleEntities, meshesDrawn, targetWidth, targetHeight);
            loggedSummaryDefaultEmpty = true;
        }
    } else if (!loggedSummaryOther) {
        spdlog::warn("Graphics(Forge): renderLayer summary layer={} target={} useSwapchain={} brightness={} entities={} meshes={} size={}x{}",
                     static_cast<int>(layer), static_cast<unsigned int>(target),
                     useSwapchain ? "yes" : "no",
                     wantsBrightness ? "yes" : "no",
                     visibleEntities, meshesDrawn, targetWidth, targetHeight);
        loggedSummaryOther = true;
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
            spdlog::warn("Graphics(Forge): UI overlay skipped visible={} valid={}",
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
        static bool loggedOnce = false;
        if (!loggedOnce) {
            spdlog::warn("Graphics(Forge): UI overlay texture resolve failed (token={}, size={}x{}).",
                         static_cast<unsigned long long>(uiOverlayTexture_.id),
                         uiOverlayTexture_.width,
                         uiOverlayTexture_.height);
            loggedOnce = true;
        }
        return;
    }
    const uint32_t setIndex = frameIndex_ % kDescriptorSetRingSize;

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
    const uint32_t setIndex = frameIndex_ % kDescriptorSetRingSize;

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
    updateDescriptorSet(renderer_, setIndex, uiOverlayDescriptorSet_, 3, params);

    cmdSetViewport(cmd_, 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f);
    cmdSetScissor(cmd_, 0, 0, width, height);
    cmdBindPipeline(cmd_, uiOverlayPipeline_);
    cmdBindDescriptorSet(cmd_, setIndex, uiOverlayDescriptorSet_);

    uint32_t stride = sizeof(UiVertex);
    uint64_t offset = 0;
    cmdBindVertexBuffer(cmd_, 1, &uiOverlayVertexBuffer_, &stride, &offset);
    cmdBindIndexBuffer(cmd_, uiOverlayIndexBuffer_, INDEX_TYPE_UINT16, 0);
    cmdDrawIndexed(cmd_, 6, 0, 0);
}

void ForgeBackend::setBrightness(float brightness) {
    brightness_ = brightness;
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
        // Vulkan clip space depth is [0, 1]. Allow forcing LH for debugging camera handedness.
        const char* forceLh = std::getenv("BZ3_FORGE_USE_LH");
        if (forceLh && forceLh[0] == '1') {
            return glm::perspectiveLH_ZO(glm::radians(fovDegrees), aspectRatio, nearPlane, farPlane);
        }
        return glm::perspectiveRH_ZO(glm::radians(fovDegrees), aspectRatio, nearPlane, farPlane);
    }
    const char* forceLh = std::getenv("BZ3_FORGE_USE_LH");
    if (forceLh && forceLh[0] == '1') {
        return glm::orthoLH_ZO(orthoLeft, orthoRight, orthoBottom, orthoTop, nearPlane, farPlane);
    }
    return glm::orthoRH_ZO(orthoLeft, orthoRight, orthoBottom, orthoTop, nearPlane, farPlane);
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
    auto vsBytes = bz::file::ReadFileBytes(vsPath);
    auto fsBytes = bz::file::ReadFileBytes(fsPath);
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
    setDesc.mMaxSets = kDescriptorSetRingSize;
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
    const bool forceOpaque = []() {
        const char* flag = std::getenv("BZ3_FORGE_DEBUG_OPAQUE");
        return flag && flag[0] == '1';
    }();
    blend.mColorWriteMasks[0] = COLOR_MASK_ALL;
    blend.mRenderTargetMask = BLEND_STATE_TARGET_ALL;
    blend.mIndependentBlend = false;
    if (!forceOpaque) {
        blend.mSrcFactors[0] = BC_SRC_ALPHA;
        blend.mDstFactors[0] = BC_ONE_MINUS_SRC_ALPHA;
        blend.mSrcAlphaFactors[0] = BC_SRC_ALPHA;
        blend.mDstAlphaFactors[0] = BC_ONE_MINUS_SRC_ALPHA;
    }

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

void ForgeBackend::ensureBrightnessResources() {
    if (!renderer_) {
        return;
    }
    if (brightnessPipeline_ && brightnessDescriptorSet_ && brightnessVertexBuffer_
        && brightnessIndexBuffer_ && brightnessUniformBuffer_) {
        return;
    }

    const std::filesystem::path shaderDir = bz::data::Resolve("forge/shaders");
    const auto vsPath = shaderDir / "brightness.vert.spv";
    const auto fsPath = shaderDir / "brightness.frag.spv";
    auto vsBytes = bz::file::ReadFileBytes(vsPath);
    auto fsBytes = bz::file::ReadFileBytes(fsPath);
    if (vsBytes.empty() || fsBytes.empty()) {
        spdlog::error("Graphics(Forge): missing brightness shaders '{}', '{}'", vsPath.string(), fsPath.string());
        return;
    }

    BinaryShaderDesc shaderDesc{};
    shaderDesc.mStages = SHADER_STAGE_VERT | SHADER_STAGE_FRAG;
    shaderDesc.mOwnByteCode = false;
    shaderDesc.mVert = { "brightness.vert", vsBytes.data(), static_cast<uint32_t>(vsBytes.size()), "main" };
    shaderDesc.mFrag = { "brightness.frag", fsBytes.data(), static_cast<uint32_t>(fsBytes.size()), "main" };
    addShaderBinary(renderer_, &shaderDesc, &brightnessShader_);
    if (!brightnessShader_) {
        spdlog::error("Graphics(Forge): failed to create brightness shader");
        return;
    }

    SamplerDesc samplerDesc{};
    samplerDesc.mMinFilter = FILTER_LINEAR;
    samplerDesc.mMagFilter = FILTER_LINEAR;
    samplerDesc.mMipMapMode = MIPMAP_MODE_LINEAR;
    samplerDesc.mAddressU = ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerDesc.mAddressV = ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerDesc.mAddressW = ADDRESS_MODE_CLAMP_TO_EDGE;
    addSampler(renderer_, &samplerDesc, &brightnessSampler_);

    brightnessDescriptors_[0].mType = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    brightnessDescriptors_[0].mCount = 1;
    brightnessDescriptors_[0].mOffset = 0;
    brightnessDescriptors_[1].mType = DESCRIPTOR_TYPE_TEXTURE;
    brightnessDescriptors_[1].mCount = 1;
    brightnessDescriptors_[1].mOffset = 1;
    brightnessDescriptors_[2].mType = DESCRIPTOR_TYPE_SAMPLER;
    brightnessDescriptors_[2].mCount = 1;
    brightnessDescriptors_[2].mOffset = 2;

    DescriptorSetDesc setDesc{};
    setDesc.mIndex = 0;
    setDesc.mMaxSets = kDescriptorSetRingSize;
    setDesc.mDescriptorCount = 3;
    setDesc.pDescriptors = brightnessDescriptors_;
    addDescriptorSet(renderer_, &setDesc, &brightnessDescriptorSet_);

    BufferLoadDesc vbDesc{};
    vbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
    vbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
    vbDesc.mDesc.mSize = sizeof(float) * 4 * 4;
    vbDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
    vbDesc.mDesc.pName = "Forge Brightness VB";
    vbDesc.ppBuffer = &brightnessVertexBuffer_;
    addResource(&vbDesc, nullptr);

    BufferLoadDesc ibDesc = vbDesc;
    ibDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_INDEX_BUFFER;
    ibDesc.mDesc.mSize = sizeof(uint16_t) * 6;
    ibDesc.mDesc.pName = "Forge Brightness IB";
    ibDesc.ppBuffer = &brightnessIndexBuffer_;
    addResource(&ibDesc, nullptr);

    BufferLoadDesc ubDesc{};
    ubDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    ubDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
    ubDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
    ubDesc.mDesc.mSize = sizeof(float) * 8;
    ubDesc.mDesc.pName = "Forge Brightness UB";
    ubDesc.ppBuffer = &brightnessUniformBuffer_;
    addResource(&ubDesc, nullptr);

    VertexLayout layout{};
    layout.mBindingCount = 1;
    layout.mAttribCount = 2;
    layout.mBindings[0].mStride = sizeof(float) * 4;
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

    BlendStateDesc blend{};
    blend.mSrcFactors[0] = BC_ONE;
    blend.mDstFactors[0] = BC_ZERO;
    blend.mSrcAlphaFactors[0] = BC_ONE;
    blend.mDstAlphaFactors[0] = BC_ZERO;
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
    pipelineDesc.mGraphicsDesc.pShaderProgram = brightnessShader_;
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
    layoutDesc.pDescriptors = brightnessDescriptors_;
    layoutDesc.mDescriptorCount = 3;
    layoutDesc.pStaticSamplers = nullptr;
    layoutDesc.mStaticSamplerCount = 0;
    const DescriptorSetLayoutDesc* layoutPtrs[] = { &layoutDesc };
    pipelineDesc.pLayouts = layoutPtrs;
    pipelineDesc.mLayoutCount = 1;

    addPipeline(renderer_, &pipelineDesc, &brightnessPipeline_);
}

void ForgeBackend::destroyBrightnessResources() {
    if (!renderer_) {
        return;
    }
    if (brightnessPipeline_) {
        removePipeline(renderer_, brightnessPipeline_);
        brightnessPipeline_ = nullptr;
    }
    if (brightnessShader_) {
        removeShader(renderer_, brightnessShader_);
        brightnessShader_ = nullptr;
    }
    if (brightnessDescriptorSet_) {
        removeDescriptorSet(renderer_, brightnessDescriptorSet_);
        brightnessDescriptorSet_ = nullptr;
    }
    if (brightnessSampler_) {
        removeSampler(renderer_, brightnessSampler_);
        brightnessSampler_ = nullptr;
    }
    if (brightnessVertexBuffer_) {
        removeResource(brightnessVertexBuffer_);
        brightnessVertexBuffer_ = nullptr;
    }
    if (brightnessIndexBuffer_) {
        removeResource(brightnessIndexBuffer_);
        brightnessIndexBuffer_ = nullptr;
    }
    if (brightnessUniformBuffer_) {
        removeResource(brightnessUniformBuffer_);
        brightnessUniformBuffer_ = nullptr;
    }
}

void ForgeBackend::ensureSceneTarget(int width, int height) {
    if (!renderer_ || width <= 0 || height <= 0) {
        return;
    }
    if (sceneTarget_ && sceneTargetWidth_ == width && sceneTargetHeight_ == height) {
        return;
    }
    destroySceneTarget();

    RenderTargetDesc rtDesc{};
    rtDesc.mWidth = static_cast<uint32_t>(width);
    rtDesc.mHeight = static_cast<uint32_t>(height);
    rtDesc.mDepth = 1;
    rtDesc.mArraySize = 1;
    rtDesc.mMipLevels = 1;
    rtDesc.mSampleCount = SAMPLE_COUNT_1;
    rtDesc.mSampleQuality = 0;
    rtDesc.mFormat = swapChain_ ? swapChain_->ppRenderTargets[0]->mFormat : TinyImageFormat_R8G8B8A8_UNORM;
    rtDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
    rtDesc.mStartState = RESOURCE_STATE_RENDER_TARGET;
    rtDesc.pName = "Forge Scene RenderTarget";
    addRenderTarget(renderer_, &rtDesc, &sceneTarget_);
    sceneTargetWidth_ = width;
    sceneTargetHeight_ = height;
}

void ForgeBackend::destroySceneTarget() {
    if (renderer_ && sceneTarget_) {
        removeRenderTarget(renderer_, sceneTarget_);
        sceneTarget_ = nullptr;
    }
    sceneTargetWidth_ = 0;
    sceneTargetHeight_ = 0;
}

void ForgeBackend::renderBrightnessPass() {
    if (!cmd_ || !renderer_ || !swapChain_ || !sceneTarget_) {
        return;
    }
    ensureBrightnessResources();
    if (!brightnessPipeline_ || !brightnessDescriptorSet_ || !brightnessVertexBuffer_
        || !brightnessIndexBuffer_ || !brightnessUniformBuffer_ || !brightnessSampler_) {
        return;
    }

    RenderTarget* backBuffer = swapChain_->ppRenderTargets[frameIndex_];
    BindRenderTargetsDesc bindDesc{};
    bindDesc.mRenderTargetCount = 1;
    bindDesc.mRenderTargets[0].pRenderTarget = backBuffer;
    bindDesc.mRenderTargets[0].mLoadAction = LOAD_ACTION_CLEAR;
    bindDesc.mRenderTargets[0].mStoreAction = STORE_ACTION_STORE;
    bindDesc.mRenderTargets[0].mClearValue = {0.0f, 0.0f, 0.0f, 1.0f};
    bindDesc.mRenderTargets[0].mOverrideClearValue = 1;
    bindDesc.mDepthStencil.pDepthStencil = nullptr;
    bindDesc.mDepthStencil.mLoadAction = LOAD_ACTION_DONTCARE;
    bindDesc.mDepthStencil.mStoreAction = STORE_ACTION_DONTCARE;
    cmdBindRenderTargets(cmd_, &bindDesc);

    const uint32_t width = framebufferWidth > 0 ? static_cast<uint32_t>(framebufferWidth) : 1u;
    const uint32_t height = framebufferHeight > 0 ? static_cast<uint32_t>(framebufferHeight) : 1u;

    struct BrightnessConstants {
        float scaleBias[4];
        float brightness;
        float pad[3];
    } constants{};
    constants.scaleBias[0] = 2.0f / static_cast<float>(width);
    constants.scaleBias[1] = -2.0f / static_cast<float>(height);
    constants.scaleBias[2] = -1.0f;
    constants.scaleBias[3] = 1.0f;
    constants.brightness = brightness_;
    BufferUpdateDesc cbUpdate = { brightnessUniformBuffer_ };
    beginUpdateResource(&cbUpdate);
    std::memcpy(cbUpdate.pMappedData, &constants, sizeof(constants));
    endUpdateResource(&cbUpdate);

    struct BrightnessVertex {
        float x;
        float y;
        float u;
        float v;
    };
    BrightnessVertex vertices[4] = {
        {0.0f, 0.0f, 0.0f, 0.0f},
        {static_cast<float>(width), 0.0f, 1.0f, 0.0f},
        {static_cast<float>(width), static_cast<float>(height), 1.0f, 1.0f},
        {0.0f, static_cast<float>(height), 0.0f, 1.0f},
    };
    const uint16_t indices[6] = {0, 1, 2, 0, 2, 3};

    BufferUpdateDesc vbUpdate = { brightnessVertexBuffer_ };
    beginUpdateResource(&vbUpdate);
    std::memcpy(vbUpdate.pMappedData, vertices, sizeof(vertices));
    endUpdateResource(&vbUpdate);

    BufferUpdateDesc ibUpdate = { brightnessIndexBuffer_ };
    beginUpdateResource(&ibUpdate);
    std::memcpy(ibUpdate.pMappedData, indices, sizeof(indices));
    endUpdateResource(&ibUpdate);

    DescriptorData params[3] = {};
    params[0].mIndex = 0;
    params[0].ppBuffers = &brightnessUniformBuffer_;
    params[1].mIndex = 1;
    params[1].ppTextures = &sceneTarget_->pTexture;
    params[2].mIndex = 2;
    params[2].ppSamplers = &brightnessSampler_;
    updateDescriptorSet(renderer_, setIndex, brightnessDescriptorSet_, 3, params);

    cmdSetViewport(cmd_, 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f);
    cmdSetScissor(cmd_, 0, 0, width, height);
    cmdBindPipeline(cmd_, brightnessPipeline_);
    cmdBindDescriptorSet(cmd_, setIndex, brightnessDescriptorSet_);

    uint32_t stride = sizeof(BrightnessVertex);
    uint64_t offset = 0;
    cmdBindVertexBuffer(cmd_, 1, &brightnessVertexBuffer_, &stride, &offset);
    cmdBindIndexBuffer(cmd_, brightnessIndexBuffer_, INDEX_TYPE_UINT16, 0);
    cmdDrawIndexed(cmd_, 6, 0, 0);
    cmdBindRenderTargets(cmd_, nullptr);
}

void ForgeBackend::ensureMeshResources() {
    if (!renderer_) {
        return;
    }
    if (meshPipeline_ && meshPipelineOffscreen_ && meshDescriptorSet_ && meshUniformBuffer_ && meshSampler_) {
        return;
    }

    const std::filesystem::path shaderDir = bz::data::Resolve("forge/shaders");
    const bool debugSolid = []() {
        const char* flag = std::getenv("BZ3_FORGE_DEBUG_SOLID_SHADER");
        return flag && flag[0] == '1';
    }();
    const auto vsPath = shaderDir / (debugSolid ? "mesh_debug.vert.spv" : "mesh.vert.spv");
    const auto fsPath = shaderDir / (debugSolid ? "mesh_debug.frag.spv" : "mesh.frag.spv");
    auto vsBytes = bz::file::ReadFileBytes(vsPath);
    auto fsBytes = bz::file::ReadFileBytes(fsPath);
    if (vsBytes.empty() || fsBytes.empty()) {
        spdlog::error("Graphics(Forge): missing mesh shaders '{}', '{}'", vsPath.string(), fsPath.string());
        return;
    }
    spdlog::warn("Graphics(Forge): mesh shader selection vs='{}' fs='{}' bytes=({}, {})",
                 vsPath.string(), fsPath.string(), vsBytes.size(), fsBytes.size());

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
        setDesc.mMaxSets = kDescriptorSetRingSize;
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
