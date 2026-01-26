#include "engine/graphics/backends/bgfx/backend.hpp"

#include "engine/common/data_path_resolver.hpp"
#include "platform/window.hpp"

#include <bgfx/bgfx.h>
#include <bgfx/platform.h>
#include <bx/math.h>
#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <spdlog/spdlog.h>
#include <fstream>
#include <cstdlib>

#if defined(BZ3_WINDOW_BACKEND_GLFW)
#define GLFW_EXPOSE_NATIVE_X11
#define GLFW_EXPOSE_NATIVE_GLX
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#elif defined(BZ3_WINDOW_BACKEND_SDL)
#include <SDL3/SDL.h>
#include <SDL3/SDL_properties.h>
#include <SDL3/SDL_video.h>
#endif

namespace {
struct NativeWindowInfo {
    void* nwh = nullptr;
    void* ndt = nullptr;
    void* context = nullptr;
    bgfx::NativeWindowHandleType::Enum handleType = bgfx::NativeWindowHandleType::Default;
};

NativeWindowInfo getNativeWindowInfo(platform::Window* window) {
    if (!window) {
        return {};
    }
    void* handle = window->nativeHandle();
#if defined(BZ3_WINDOW_BACKEND_SDL)
    auto* sdlWindow = static_cast<SDL_Window*>(handle);
    if (!sdlWindow) {
        return {};
    }
    NativeWindowInfo info{};
    const SDL_PropertiesID props = SDL_GetWindowProperties(sdlWindow);
    if (props != 0) {
        if (void* wlDisplay = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER, nullptr)) {
            info.ndt = wlDisplay;
            void* wlSurface = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, nullptr);
            void* wlEglWindow = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WAYLAND_EGL_WINDOW_POINTER, nullptr);
            spdlog::info("Graphics(Bgfx): Wayland handles display={} surface={} egl_window={}",
                         info.ndt, wlSurface, wlEglWindow);
            if (wlSurface) {
                info.nwh = wlSurface;
                info.handleType = bgfx::NativeWindowHandleType::Wayland;
                spdlog::info("Graphics(Bgfx): using Wayland surface handle");
                return info;
            }
            if (wlEglWindow) {
                info.nwh = wlEglWindow;
                info.handleType = bgfx::NativeWindowHandleType::Default;
                spdlog::info("Graphics(Bgfx): using Wayland egl_window handle (fallback)");
                return info;
            }
            spdlog::info("Graphics(Bgfx): Wayland display found but no surface/egl_window");
            return {};
        }
        if (const Sint64 x11Window = SDL_GetNumberProperty(props, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0)) {
            info.nwh = reinterpret_cast<void*>(static_cast<uintptr_t>(x11Window));
            return info;
        }
    }
    return {};
#elif defined(BZ3_WINDOW_BACKEND_GLFW)
    auto* glfwWindow = static_cast<GLFWwindow*>(handle);
    if (!glfwWindow) {
        return {};
    }
    NativeWindowInfo info{};
    info.ndt = glfwGetX11Display();
    const auto x11Window = glfwGetX11Window(glfwWindow);
    if (x11Window != 0) {
        info.nwh = reinterpret_cast<void*>(static_cast<uintptr_t>(x11Window));
    } else {
        info.nwh = glfwWindow;
    }
    info.context = glfwGetGLXContext(glfwWindow);
    return info;
#else
    return {handle, nullptr, nullptr};
#endif
}

std::vector<uint8_t> readFileToBytes(const std::filesystem::path& path) {
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
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
        return {};
    }
    return buffer;
}

bgfx::ShaderHandle loadShader(const std::filesystem::path& path) {
    auto bytes = readFileToBytes(path);
    if (bytes.empty()) {
        spdlog::error("Graphics(Bgfx): failed to read shader '{}'", path.string());
        return BGFX_INVALID_HANDLE;
    }
    const bgfx::Memory* mem = bgfx::copy(bytes.data(), static_cast<uint32_t>(bytes.size()));
    return bgfx::createShader(mem);
}

struct TestVertex {
    float x;
    float y;
    float z;
    uint32_t abgr;
};
} // namespace

namespace graphics_backend {

BgfxBackend::BgfxBackend(platform::Window& windowIn)
    : window(&windowIn) {
    spdlog::info("Graphics(Bgfx): ctor begin");
    if (window) {
        window->getFramebufferSize(framebufferWidth, framebufferHeight);
        if (framebufferWidth <= 0) {
            framebufferWidth = 1;
        }
        if (framebufferHeight <= 0) {
            framebufferHeight = 1;
        }
        window->makeContextCurrent();
    }

    const auto nativeInfo = getNativeWindowInfo(window);
    bgfx::PlatformData pd{};
    pd.ndt = nativeInfo.ndt;
    pd.nwh = nativeInfo.nwh;
    pd.context = nullptr;
#if defined(BZ3_WINDOW_BACKEND_SDL)
    pd.type = nativeInfo.handleType;
#endif
    spdlog::info("Graphics(Bgfx): platform nwh={} ndt={} ctx={}", pd.nwh, pd.ndt, pd.context);

    if (!pd.ndt || !pd.nwh) {
        spdlog::error("Graphics(Bgfx): missing native display/window handle (ndt={}, nwh={})", pd.ndt, pd.nwh);
        return;
    }

    bgfx::Init init{};
    init.type = bgfx::RendererType::Vulkan;
    init.vendorId = BGFX_PCI_ID_NONE;
    init.platformData = pd;
    init.resolution.width = static_cast<uint32_t>(framebufferWidth);
    init.resolution.height = static_cast<uint32_t>(framebufferHeight);
    init.resolution.reset = BGFX_RESET_VSYNC;
    initialized = bgfx::init(init);

    spdlog::info("Graphics(Bgfx): init result={} size={}x{}", initialized, framebufferWidth, framebufferHeight);
    if (initialized) {
        bgfx::setViewClear(0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x0d1620ff, 1.0f, 0);
        bgfx::setViewRect(0, 0, 0, static_cast<uint16_t>(framebufferWidth), static_cast<uint16_t>(framebufferHeight));
        bgfx::setViewTransform(0, nullptr, nullptr);
        buildTestResources();
        spdlog::info("Graphics(Bgfx): init ok renderer={} testReady={}",
                     static_cast<int>(bgfx::getRendererType()),
                     testReady);
    }
}

BgfxBackend::~BgfxBackend() {
    if (initialized) {
        if (bgfx::isValid(testVertexBuffer)) {
            bgfx::destroy(testVertexBuffer);
        }
        if (bgfx::isValid(testIndexBuffer)) {
            bgfx::destroy(testIndexBuffer);
        }
        if (bgfx::isValid(testProgram)) {
            bgfx::destroy(testProgram);
        }
        bgfx::shutdown();
    }
}

void BgfxBackend::beginFrame() {
    if (!initialized) {
        return;
    }
    static uint64_t frameCounter = 0;
    if (frameCounter < 120) {
        spdlog::info("Graphics(Bgfx): beginFrame frame={} testReady={} vb={} ib={} prog={}",
                     frameCounter,
                     testReady,
                     bgfx::isValid(testVertexBuffer),
                     bgfx::isValid(testIndexBuffer),
                     bgfx::isValid(testProgram));
    }
    if (testReady) {
        bgfx::setViewTransform(0, nullptr, nullptr);
        bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
        bgfx::setVertexBuffer(0, testVertexBuffer);
        bgfx::setIndexBuffer(testIndexBuffer);
        bgfx::submit(0, testProgram);
    }
    bgfx::touch(0);

    const bgfx::Stats* stats = bgfx::getStats();
    if (stats) {
        if (frameCounter < 120) {
            spdlog::info(
                "Graphics(Bgfx): stats drawCalls={} gpuFrame={} gpuTime={} cpuTime={}",
                stats->numDraw,
                stats->gpuFrameNum,
                stats->gpuTimeEnd - stats->gpuTimeBegin,
                stats->cpuTimeEnd - stats->cpuTimeBegin);
        }
    }
    ++frameCounter;
}

void BgfxBackend::endFrame() {
    if (!initialized) {
        return;
    }
    bgfx::frame();
}

void BgfxBackend::resize(int width, int height) {
    framebufferWidth = std::max(1, width);
    framebufferHeight = std::max(1, height);
    if (initialized) {
        bgfx::reset(static_cast<uint32_t>(framebufferWidth),
                    static_cast<uint32_t>(framebufferHeight),
                    BGFX_RESET_VSYNC);
        bgfx::setViewRect(0, 0, 0, static_cast<uint16_t>(framebufferWidth), static_cast<uint16_t>(framebufferHeight));
    }
}

graphics::EntityId BgfxBackend::createEntity(graphics::LayerId layer) {
    const graphics::EntityId id = nextEntityId++;
    EntityRecord record;
    record.layer = layer;
    entities[id] = record;
    return id;
}

graphics::EntityId BgfxBackend::createModelEntity(const std::filesystem::path& modelPath,
                                                  graphics::LayerId layer,
                                                  graphics::MaterialId materialOverride) {
    const graphics::EntityId id = createEntity(layer);
    setEntityModel(id, modelPath, materialOverride);
    return id;
}

graphics::EntityId BgfxBackend::createMeshEntity(graphics::MeshId mesh,
                                                 graphics::LayerId layer,
                                                 graphics::MaterialId materialOverride) {
    const graphics::EntityId id = createEntity(layer);
    setEntityMesh(id, mesh, materialOverride);
    return id;
}

void BgfxBackend::setEntityModel(graphics::EntityId entity,
                                 const std::filesystem::path& modelPath,
                                 graphics::MaterialId materialOverride) {
    auto it = entities.find(entity);
    if (it == entities.end()) {
        return;
    }
    it->second.modelPath = modelPath;
    it->second.material = materialOverride;
}

void BgfxBackend::setEntityMesh(graphics::EntityId entity,
                                graphics::MeshId mesh,
                                graphics::MaterialId materialOverride) {
    auto it = entities.find(entity);
    if (it == entities.end()) {
        return;
    }
    it->second.mesh = mesh;
    it->second.material = materialOverride;
}

void BgfxBackend::destroyEntity(graphics::EntityId entity) {
    entities.erase(entity);
}

graphics::MeshId BgfxBackend::createMesh(const graphics::MeshData& mesh) {
    const graphics::MeshId id = nextMeshId++;
    meshes[id] = mesh;
    return id;
}

void BgfxBackend::destroyMesh(graphics::MeshId mesh) {
    meshes.erase(mesh);
}

graphics::MaterialId BgfxBackend::createMaterial(const graphics::MaterialDesc& material) {
    const graphics::MaterialId id = nextMaterialId++;
    materials[id] = material;
    return id;
}

void BgfxBackend::updateMaterial(graphics::MaterialId material, const graphics::MaterialDesc& desc) {
    auto it = materials.find(material);
    if (it == materials.end()) {
        return;
    }
    it->second = desc;
}

void BgfxBackend::destroyMaterial(graphics::MaterialId material) {
    materials.erase(material);
}

void BgfxBackend::setMaterialFloat(graphics::MaterialId, std::string_view, float) {
}

graphics::RenderTargetId BgfxBackend::createRenderTarget(const graphics::RenderTargetDesc& desc) {
    const graphics::RenderTargetId id = nextRenderTargetId++;
    RenderTargetRecord record;
    record.desc = desc;
    renderTargets[id] = record;
    return id;
}

void BgfxBackend::destroyRenderTarget(graphics::RenderTargetId target) {
    renderTargets.erase(target);
}

void BgfxBackend::renderLayer(graphics::LayerId, graphics::RenderTargetId) {}

unsigned int BgfxBackend::getRenderTargetTextureId(graphics::RenderTargetId) const {
    return 0u;
}

void BgfxBackend::setPosition(graphics::EntityId entity, const glm::vec3& position) {
    auto it = entities.find(entity);
    if (it == entities.end()) {
        return;
    }
    it->second.position = position;
}

void BgfxBackend::setRotation(graphics::EntityId entity, const glm::quat& rotation) {
    auto it = entities.find(entity);
    if (it == entities.end()) {
        return;
    }
    it->second.rotation = rotation;
}

void BgfxBackend::setScale(graphics::EntityId entity, const glm::vec3& scale) {
    auto it = entities.find(entity);
    if (it == entities.end()) {
        return;
    }
    it->second.scale = scale;
}

void BgfxBackend::setVisible(graphics::EntityId entity, bool visible) {
    auto it = entities.find(entity);
    if (it == entities.end()) {
        return;
    }
    it->second.visible = visible;
}

void BgfxBackend::setTransparency(graphics::EntityId entity, bool transparency) {
    auto it = entities.find(entity);
    if (it == entities.end()) {
        return;
    }
    it->second.transparent = transparency;
}

void BgfxBackend::setCameraPosition(const glm::vec3& position) {
    cameraPosition = position;
}

void BgfxBackend::setCameraRotation(const glm::quat& rotation) {
    cameraRotation = rotation;
}

void BgfxBackend::setPerspective(float fovDeg, float aspect, float nearPlaneIn, float farPlaneIn) {
    usePerspective = true;
    fovDegrees = fovDeg;
    aspectRatio = aspect;
    nearPlane = nearPlaneIn;
    farPlane = farPlaneIn;
}

void BgfxBackend::setOrthographic(float left, float right, float top, float bottom, float nearPlaneIn, float farPlaneIn) {
    usePerspective = false;
    orthoLeft = left;
    orthoRight = right;
    orthoTop = top;
    orthoBottom = bottom;
    nearPlane = nearPlaneIn;
    farPlane = farPlaneIn;
}

glm::mat4 BgfxBackend::computeViewMatrix() const {
    const glm::mat4 rotation = glm::mat4_cast(glm::conjugate(cameraRotation));
    const glm::mat4 translation = glm::translate(glm::mat4(1.0f), -cameraPosition);
    return rotation * translation;
}

glm::mat4 BgfxBackend::computeProjectionMatrix() const {
    if (usePerspective) {
        return glm::perspective(glm::radians(fovDegrees), aspectRatio, nearPlane, farPlane);
    }
    return glm::ortho(orthoLeft, orthoRight, orthoBottom, orthoTop, nearPlane, farPlane);
}

glm::mat4 BgfxBackend::getViewProjectionMatrix() const {
    return computeProjectionMatrix() * computeViewMatrix();
}

glm::mat4 BgfxBackend::getViewMatrix() const {
    return computeViewMatrix();
}

glm::mat4 BgfxBackend::getProjectionMatrix() const {
    return computeProjectionMatrix();
}

glm::vec3 BgfxBackend::getCameraPosition() const {
    return cameraPosition;
}

glm::vec3 BgfxBackend::getCameraForward() const {
    const glm::vec3 forward = cameraRotation * glm::vec3(0.0f, 0.0f, -1.0f);
    return glm::normalize(forward);
}

void BgfxBackend::buildTestResources() {
    if (!initialized || testReady) {
        return;
    }

    const std::filesystem::path vsPath = bz::data::Resolve("bgfx/shaders/bin/vs_triangle.bin");
    const std::filesystem::path fsPath = bz::data::Resolve("bgfx/shaders/bin/fs_triangle.bin");

    if (!std::filesystem::exists(vsPath) || !std::filesystem::exists(fsPath)) {
        spdlog::error("Graphics(Bgfx): missing shader binaries '{}', '{}'", vsPath.string(), fsPath.string());
        return;
    }

    bgfx::ShaderHandle vsh = loadShader(vsPath);
    bgfx::ShaderHandle fsh = loadShader(fsPath);
    if (!bgfx::isValid(vsh) || !bgfx::isValid(fsh)) {
        if (bgfx::isValid(vsh)) {
            bgfx::destroy(vsh);
        }
        if (bgfx::isValid(fsh)) {
            bgfx::destroy(fsh);
        }
        return;
    }
    testProgram = bgfx::createProgram(vsh, fsh, true);
    if (!bgfx::isValid(testProgram)) {
        spdlog::error("Graphics(Bgfx): failed to create shader program");
        return;
    }

    testLayout.begin()
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
        .end();

    static const TestVertex verts[] = {
        {-0.6f, -0.4f, 0.0f, 0xff0000ff},
        {0.6f, -0.4f, 0.0f, 0xff00ff00},
        {0.0f, 0.6f, 0.0f, 0xffff0000},
    };
    static const uint16_t indices[] = {0, 1, 2};

    testVertexBuffer = bgfx::createVertexBuffer(bgfx::copy(verts, sizeof(verts)), testLayout);
    testIndexBuffer = bgfx::createIndexBuffer(bgfx::copy(indices, sizeof(indices)));

    testReady = bgfx::isValid(testVertexBuffer) && bgfx::isValid(testIndexBuffer);
    if (!testReady) {
        spdlog::error("Graphics(Bgfx): failed to create test geometry");
    }
}

} // namespace graphics_backend
