#include "engine/graphics/backends/filament/backend.hpp"

#include "platform/window.hpp"
#ifdef assert_invariant
#undef assert_invariant
#endif
#include "common/config_helpers.hpp"
#include "common/data_path_resolver.hpp"
#include "spdlog/spdlog.h"

#include <filament/LightManager.h>
#include <filament/RenderableManager.h>
#include <filament/TransformManager.h>
#include <filament/Viewport.h>
#include <gltfio/MaterialProvider.h>
#include <gltfio/TextureProvider.h>
#include <gltfio/materials/uberarchive.h>
#include <ktxreader/Ktx1Reader.h>
#define FILAMENT_SUPPORTS_WAYLAND 1
#include <bluevk/BlueVK.h>
#include <backend/platforms/VulkanPlatform.h>
#include <wayland-client-core.h>
#include <math/mat4.h>
#include <math/vec4.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <vector>

#if defined(BZ3_WINDOW_BACKEND_SDL3)
#include <SDL3/SDL.h>
#include <SDL3/SDL_properties.h>
#include <SDL3/SDL_video.h>
#endif

namespace {
using graphics_backend::filament_backend_detail::WaylandNativeWindow;
filament::math::mat4f toFilamentMat4(const glm::mat4& m) {
    filament::math::mat4f out;
    for (int c = 0; c < 4; ++c) {
        for (int r = 0; r < 4; ++r) {
            out[c][r] = m[c][r];
        }
    }
    return out;
}

struct UiQuadVertex {
    float x, y, z;
    float u0, v0;
    float u1, v1;
    float r, g, b, a;
};

std::vector<uint8_t> readFileToBytes(const std::filesystem::path& path) {
    if (path.empty()) {
        return {};
    }

    std::ifstream file(path, std::ios::binary);
    if (!file) {
        spdlog::error("Graphics(Filament): Failed to open file '{}'", path.string());
        return {};
    }

    file.seekg(0, std::ios::end);
    const auto size = file.tellg();
    if (size <= 0) {
        return {};
    }
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> data(static_cast<size_t>(size));
    file.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(data.size()));
    return data;
}

struct IblConfig {
    std::filesystem::path iblPath;
    std::filesystem::path skyboxPath;
    float intensity = 30000.0f;
    bool hasIbl = false;
    bool hasSkybox = false;
    bool hasSkyboxColor = false;
    filament::math::float4 skyboxColor{0.05f, 0.08f, 0.12f, 1.0f};
    filament::math::float3 keyLightColor{1.0f, 1.0f, 1.0f};
    filament::math::float3 fillLightColor{0.85f, 0.9f, 1.0f};
    float keyLightIntensity = 60000.0f;
    float fillLightIntensity = 40000.0f;
};

IblConfig resolveIblConfig() {
    IblConfig config;
    const std::string selected = bz::data::ReadStringConfig("filament.ibl.Selected", "lightroom");
    if (auto options = bz::data::ConfigValueCopy("filament.ibl.Options"); options && options->is_array()) {
        for (const auto& opt : *options) {
            if (!opt.is_object()) {
                continue;
            }
            const auto nameIt = opt.find("Name");
            if (nameIt == opt.end() || !nameIt->is_string()) {
                continue;
            }
            if (nameIt->get<std::string>() != selected) {
                continue;
            }
            const auto skyboxIt = opt.find("Skybox");
            if (skyboxIt != opt.end() && skyboxIt->is_string()) {
                config.skyboxPath = bz::data::Resolve(skyboxIt->get<std::string>());
                config.hasSkybox = true;
            }
            const auto iblIt = opt.find("IndirectLight");
            if (iblIt != opt.end() && iblIt->is_string()) {
                config.iblPath = bz::data::Resolve(iblIt->get<std::string>());
                config.hasIbl = true;
            }
            const auto intensityIt = opt.find("Intensity");
            if (intensityIt != opt.end() && intensityIt->is_number()) {
                config.intensity = static_cast<float>(intensityIt->get<double>());
            }
            const auto keyIntensityIt = opt.find("KeyIntensity");
            if (keyIntensityIt != opt.end() && keyIntensityIt->is_number()) {
                config.keyLightIntensity = static_cast<float>(keyIntensityIt->get<double>());
            }
            const auto fillIntensityIt = opt.find("FillIntensity");
            if (fillIntensityIt != opt.end() && fillIntensityIt->is_number()) {
                config.fillLightIntensity = static_cast<float>(fillIntensityIt->get<double>());
            }
            auto getColor3 = [](const bz::json::Value& value, filament::math::float3 fallback) {
                if (!value.is_array() || value.size() < 3) {
                    return fallback;
                }
                auto getComponent = [](const bz::json::Value& v, float fb) {
                    if (v.is_number_float()) {
                        return static_cast<float>(v.get<double>());
                    }
                    if (v.is_number_integer()) {
                        return static_cast<float>(v.get<long long>());
                    }
                    return fb;
                };
                const float r = getComponent(value[0], fallback.r);
                const float g = getComponent(value[1], fallback.g);
                const float b = getComponent(value[2], fallback.b);
                return filament::math::float3{r, g, b};
            };
            const auto keyColorIt = opt.find("KeyLightColor");
            if (keyColorIt != opt.end()) {
                config.keyLightColor = getColor3(*keyColorIt, config.keyLightColor);
            }
            const auto fillColorIt = opt.find("FillLightColor");
            if (fillColorIt != opt.end()) {
                config.fillLightColor = getColor3(*fillColorIt, config.fillLightColor);
            }
            const auto colorIt = opt.find("SkyboxColor");
            if (colorIt != opt.end() && colorIt->is_array()) {
                if (colorIt->size() >= 3) {
                    auto getComponent = [](const bz::json::Value& value, float fallback) {
                        if (value.is_number_float()) {
                            return static_cast<float>(value.get<double>());
                        }
                        if (value.is_number_integer()) {
                            return static_cast<float>(value.get<long long>());
                        }
                        return fallback;
                    };
                    const float r = getComponent((*colorIt)[0], config.skyboxColor.r);
                    const float g = getComponent((*colorIt)[1], config.skyboxColor.g);
                    const float b = getComponent((*colorIt)[2], config.skyboxColor.b);
                    const float a = colorIt->size() > 3 ? getComponent((*colorIt)[3], config.skyboxColor.a) : config.skyboxColor.a;
                    config.skyboxColor = filament::math::float4{r, g, b, a};
                    config.hasSkyboxColor = true;
                }
            }
            return config;
        }
    }

    config.iblPath = bz::data::Resolve("filament/ibl/lightroom_14b/lightroom_14b_ibl.ktx");
    config.skyboxPath = bz::data::Resolve("filament/ibl/lightroom_14b/lightroom_14b_skybox.ktx");
    config.hasIbl = true;
    config.hasSkybox = true;
    return config;
}

filament::Texture* loadKtx1Texture(filament::Engine* engine, const std::filesystem::path& path, bool srgb) {
    if (!engine) {
        return nullptr;
    }
    auto bytes = readFileToBytes(path);
    if (bytes.empty()) {
        spdlog::error("Graphics(Filament): Failed to read KTX '{}'", path.string());
        return nullptr;
    }
    auto* bundle = new image::Ktx1Bundle(bytes.data(), static_cast<uint32_t>(bytes.size()));
    filament::Texture* texture = ktxreader::Ktx1Reader::createTexture(engine, bundle, srgb);
    if (!texture) {
        spdlog::error("Graphics(Filament): Failed to create KTX texture '{}'", path.string());
    }
    return texture;
}

void* getNativeWindowHandle(platform::Window* window, bool preferWaylandSurface) {
    if (!window) {
        return nullptr;
    }

    void* handle = window->nativeHandle();
#if defined(BZ3_WINDOW_BACKEND_SDL3)
    auto* sdlWindow = static_cast<SDL_Window*>(handle);
    if (!sdlWindow) {
        return nullptr;
    }

    const SDL_PropertiesID props = SDL_GetWindowProperties(sdlWindow);
    if (props != 0) {
        if (preferWaylandSurface) {
            if (void* wlSurface = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, nullptr)) {
                return wlSurface;
            }
            return nullptr;
        }

        const Sint64 x11Window = SDL_GetNumberProperty(props, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);
        if (x11Window != 0) {
            return reinterpret_cast<void*>(static_cast<uintptr_t>(x11Window));
        }

        if (void* wlSurface = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, nullptr)) {
            return wlSurface;
        }
    }

    return sdlWindow;
#else
    return handle;
#endif
}

WaylandNativeWindow* createWaylandNativeWindow(platform::Window* window, int width, int height) {
    if (!window) {
        return nullptr;
    }
#if defined(BZ3_WINDOW_BACKEND_SDL3)
    auto* sdlWindow = static_cast<SDL_Window*>(window->nativeHandle());
    if (!sdlWindow) {
        return nullptr;
    }
    const SDL_PropertiesID props = SDL_GetWindowProperties(sdlWindow);
    if (props == 0) {
        return nullptr;
    }
    auto* display = static_cast<wl_display*>(SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER, nullptr));
    auto* surface = static_cast<wl_surface*>(SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, nullptr));
    if (!display || !surface) {
        return nullptr;
    }
    auto* out = new WaylandNativeWindow();
    out->display = display;
    out->surface = surface;
    out->width = static_cast<uint32_t>(std::max(1, width));
    out->height = static_cast<uint32_t>(std::max(1, height));
    return out;
#else
    (void)width;
    (void)height;
    return nullptr;
#endif
}

class WaylandVulkanPlatform final : public filament::backend::VulkanPlatform {
public:
    ExtensionSet getSwapchainInstanceExtensions() const override {
        ExtensionSet exts;
        exts.emplace("VK_KHR_surface");
        exts.emplace("VK_KHR_wayland_surface");
        return exts;
    }

    SurfaceBundle createVkSurfaceKHR(void* nativeWindow, VkInstance instance,
            uint64_t /*flags*/) const noexcept override {
        if (!nativeWindow || instance == VK_NULL_HANDLE) {
            return {VK_NULL_HANDLE, VkExtent2D{0, 0}};
        }
        auto* wnd = static_cast<WaylandNativeWindow*>(nativeWindow);
        if (!wnd->display || !wnd->surface) {
            return {VK_NULL_HANDLE, VkExtent2D{0, 0}};
        }
        VkWaylandSurfaceCreateInfoKHR info{};
        info.sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR;
        info.display = wnd->display;
        info.surface = wnd->surface;
        VkSurfaceKHR surface = VK_NULL_HANDLE;
        VkResult res = bluevk::vkCreateWaylandSurfaceKHR(instance, &info, nullptr, &surface);
        if (res != VK_SUCCESS) {
            return {VK_NULL_HANDLE, VkExtent2D{0, 0}};
        }
        return {surface, VkExtent2D{wnd->width, wnd->height}};
    }
};
graphics_backend::FilamentBackendPreference g_filamentPreference = graphics_backend::FilamentBackendPreference::Vulkan;
} // namespace

namespace graphics_backend {

void SetFilamentBackendPreference(FilamentBackendPreference preference) {
    g_filamentPreference = preference;
}
} // namespace graphics_backend

namespace graphics_backend {

FilamentBackend::FilamentBackend(platform::Window& windowIn)
    : window(&windowIn) {
    if (window) {
        window->getFramebufferSize(framebufferWidth, framebufferHeight);
        if (framebufferHeight <= 0) {
            framebufferHeight = 1;
        }
        if (framebufferWidth <= 0) {
            framebufferWidth = 1;
        }
    }

    filament::Engine::Backend backend = filament::Engine::Backend::VULKAN;
    spdlog::info("Graphics(Filament): backend = Vulkan");

    waylandWindow = createWaylandNativeWindow(window, framebufferWidth, framebufferHeight);
    if (!waylandWindow) {
        spdlog::error("Graphics(Filament): Vulkan Wayland surface missing");
        return;
    }
    customPlatform = new WaylandVulkanPlatform();

    engine = filament::Engine::create(backend, customPlatform, nullptr);
    if (!engine) {
        spdlog::error("Graphics(Filament): Engine::create failed");
        return;
    }

    renderer = engine->createRenderer();
    if (renderer) {
        filament::Renderer::ClearOptions clear;
        clear.clear = true;
        clear.clearColor = {0.05f, 0.08f, 0.12f, 1.0f};
        clear.clearStencil = 0;
        renderer->setClearOptions(clear);
    }
    cameraEntity = utils::EntityManager::get().create();
    camera = engine->createCamera(cameraEntity);

    if (backend == filament::Engine::Backend::VULKAN) {
        nativeSwapChainHandle = waylandWindow;
        swapChain = engine->createSwapChain(nativeSwapChainHandle);
        swapChainIsNative = true;
    } else {
        swapChain = engine->createSwapChain(static_cast<uint32_t>(framebufferWidth),
                                            static_cast<uint32_t>(framebufferHeight));
        swapChainIsNative = false;
    }
    if (!swapChain) {
        spdlog::error("Graphics(Filament): createSwapChain failed");
    }

    materialProvider = filament::gltfio::createUbershaderProvider(
        engine, UBERARCHIVE_DEFAULT_DATA, UBERARCHIVE_DEFAULT_SIZE);
    textureProvider = filament::gltfio::createStbProvider(engine);
    assetLoader = filament::gltfio::AssetLoader::create({engine, materialProvider});

    if (materialProvider) {
        filament::gltfio::MaterialKey key{};
        key.doubleSided = true;
        key.unlit = true;
        key.hasBaseColorTexture = true;
        key.alphaMode = filament::gltfio::AlphaMode::BLEND;
        key.baseColorUV = 0;

        filament::gltfio::UvMap uvmap{};
        uvmap.fill(filament::gltfio::UvSet::UNUSED);
        uvmap[0] = filament::gltfio::UvSet::UV0;

        uiMaterialInstance = materialProvider->createMaterialInstance(&key, &uvmap, "ui-overlay");
    }

    if (uiMaterialInstance) {
        uiSampler = filament::TextureSampler(filament::TextureSampler::MinFilter::LINEAR,
                                             filament::TextureSampler::MagFilter::LINEAR);
        uiSampler.setWrapModeS(filament::TextureSampler::WrapMode::CLAMP_TO_EDGE);
        uiSampler.setWrapModeT(filament::TextureSampler::WrapMode::CLAMP_TO_EDGE);

        uiScene = engine->createScene();
        uiView = engine->createView();
        uiCameraEntity = utils::EntityManager::get().create();
        uiCamera = engine->createCamera(uiCameraEntity);
        uiCamera->setProjection(filament::Camera::Projection::ORTHO,
                                -1.0, 1.0, -1.0, 1.0, 0.1, 10.0);
        uiCamera->lookAt({0.0, 0.0, 1.0}, {0.0, 0.0, 0.0});
        uiView->setCamera(uiCamera);
        uiView->setScene(uiScene);
        uiView->setBlendMode(filament::View::BlendMode::TRANSLUCENT);
        uiView->setPostProcessingEnabled(false);
        filament::Viewport uiViewport{0, 0, static_cast<uint32_t>(framebufferWidth), static_cast<uint32_t>(framebufferHeight)};
        uiView->setViewport(uiViewport);

        uiQuadEntity = utils::EntityManager::get().create();
        uiVertexBuffer = filament::VertexBuffer::Builder()
            .vertexCount(4)
            .bufferCount(1)
            .attribute(filament::VertexAttribute::POSITION, 0, filament::VertexBuffer::AttributeType::FLOAT3, 0, sizeof(UiQuadVertex))
            .attribute(filament::VertexAttribute::UV0, 0, filament::VertexBuffer::AttributeType::FLOAT2, offsetof(UiQuadVertex, u0), sizeof(UiQuadVertex))
            .attribute(filament::VertexAttribute::UV1, 0, filament::VertexBuffer::AttributeType::FLOAT2, offsetof(UiQuadVertex, u1), sizeof(UiQuadVertex))
            .attribute(filament::VertexAttribute::COLOR, 0, filament::VertexBuffer::AttributeType::FLOAT4, offsetof(UiQuadVertex, r), sizeof(UiQuadVertex))
            .build(*engine);
        uiIndexBuffer = filament::IndexBuffer::Builder()
            .indexCount(6)
            .bufferType(filament::IndexBuffer::IndexType::USHORT)
            .build(*engine);

        const UiQuadVertex vertices[] = {
            {-1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f},
            { 1.0f, -1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f},
            { 1.0f,  1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f},
            {-1.0f,  1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f}
        };
        const uint16_t indices[] = {0, 1, 2, 0, 2, 3};

        uiVertexBuffer->setBufferAt(*engine, 0,
            filament::VertexBuffer::BufferDescriptor(vertices, sizeof(vertices), nullptr));
        uiIndexBuffer->setBuffer(*engine,
            filament::IndexBuffer::BufferDescriptor(indices, sizeof(indices), nullptr));

        filament::RenderableManager::Builder(1)
            .boundingBox({{0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}})
            .culling(false)
            .material(0, uiMaterialInstance)
            .geometry(0, filament::RenderableManager::PrimitiveType::TRIANGLES, uiVertexBuffer, uiIndexBuffer)
            .build(*engine, uiQuadEntity);
        uiInScene = false;
    }

    setPerspective(lastFovDegrees, lastAspect, lastNear, lastFar);
    setCameraPosition(cameraPosition);
    setCameraRotation(cameraRotation);
}

FilamentBackend::~FilamentBackend() {
    if (!engine) {
        return;
    }

    if (uiScene && uiInScene && uiQuadEntity) {
        uiScene->remove(uiQuadEntity);
    }
    if (uiQuadEntity) {
        engine->getRenderableManager().destroy(uiQuadEntity);
    }
    if (uiVertexBuffer) {
        engine->destroy(uiVertexBuffer);
        uiVertexBuffer = nullptr;
    }
    if (uiIndexBuffer) {
        engine->destroy(uiIndexBuffer);
        uiIndexBuffer = nullptr;
    }
    if (uiMaterialInstance) {
        engine->destroy(uiMaterialInstance);
        uiMaterialInstance = nullptr;
    }
    if (uiTexture) {
        engine->destroy(uiTexture);
        uiTexture = nullptr;
    }
    if (uiView) {
        engine->destroy(uiView);
        uiView = nullptr;
    }
    if (uiScene) {
        engine->destroy(uiScene);
        uiScene = nullptr;
    }
    if (uiCamera) {
        engine->destroyCameraComponent(uiCameraEntity);
        uiCamera = nullptr;
    }
    if (uiCameraEntity) {
        utils::EntityManager::get().destroy(uiCameraEntity);
        uiCameraEntity = {};
    }
    if (uiQuadEntity) {
        utils::EntityManager::get().destroy(uiQuadEntity);
        uiQuadEntity = {};
    }

    if (!assets.empty()) {
        std::vector<graphics::EntityId> assetIds;
        assetIds.reserve(assets.size());
        for (const auto& [id, asset] : assets) {
            assetIds.push_back(id);
        }
        for (const auto id : assetIds) {
            destroyAsset(id);
        }
    }

    if (!renderTargets.empty()) {
        std::vector<graphics::RenderTargetId> targetIds;
        targetIds.reserve(renderTargets.size());
        for (const auto& [id, record] : renderTargets) {
            targetIds.push_back(id);
        }
        for (const auto id : targetIds) {
            destroyRenderTarget(id);
        }
    }

    for (auto& [id, record] : entities) {
        auto it = layers.find(record.layer);
        if (it != layers.end() && it->second.scene) {
            it->second.scene->remove(record.entity);
        }
        auto& tm = engine->getTransformManager();
        auto inst = tm.getInstance(record.entity);
        if (inst.isValid()) {
            tm.destroy(record.entity);
        }
        utils::EntityManager::get().destroy(record.entity);
    }
    entities.clear();

    for (auto& [layer, state] : layers) {
        if (state.view) {
            engine->destroy(state.view);
        }
        if (state.scene) {
            engine->destroy(state.scene);
        }
    }
    layers.clear();

    if (camera) {
        engine->destroyCameraComponent(cameraEntity);
    }
    utils::EntityManager::get().destroy(cameraEntity);

    if (lightInitialized) {
        engine->destroy(lightEntity);
        utils::EntityManager::get().destroy(lightEntity);
        engine->destroy(ambientEntity);
        utils::EntityManager::get().destroy(ambientEntity);
        lightInitialized = false;
    }
    if (indirectLight) {
        engine->destroy(indirectLight);
        indirectLight = nullptr;
    }
    if (skybox) {
        engine->destroy(skybox);
        skybox = nullptr;
    }
    if (iblTexture) {
        engine->destroy(iblTexture);
        iblTexture = nullptr;
    }
    if (skyboxTexture) {
        engine->destroy(skyboxTexture);
        skyboxTexture = nullptr;
    }

    if (swapChain) {
        engine->destroy(swapChain);
    }
    if (renderer) {
        engine->destroy(renderer);
    }

    if (assetLoader) {
        filament::gltfio::AssetLoader::destroy(&assetLoader);
    }
    if (materialProvider) {
        materialProvider->destroyMaterials();
        delete materialProvider;
        materialProvider = nullptr;
    }
    if (textureProvider) {
        delete textureProvider;
        textureProvider = nullptr;
    }

    filament::Engine::destroy(&engine);
    if (customPlatform) {
        delete customPlatform;
        customPlatform = nullptr;
    }
    if (waylandWindow) {
        delete waylandWindow;
        waylandWindow = nullptr;
    }
}

void FilamentBackend::beginFrame() {
    if (!renderer || !swapChain) {
        static bool warned = false;
        if (!warned) {
            spdlog::error("Graphics(Filament): beginFrame skipped (renderer={}, swapChain={})", (void*)renderer, (void*)swapChain);
            warned = true;
        }
        return;
    }
    if (window) {
    }
    frameActive = renderer->beginFrame(swapChain);
}

void FilamentBackend::endFrame() {
    if (!renderer) {
        return;
    }
    if (frameActive) {
        renderer->endFrame();
        frameActive = false;
    }
}

void FilamentBackend::resize(int width, int height) {
    if (height <= 0) {
        height = 1;
    }
    if (width <= 0) {
        width = 1;
    }

    const bool sizeChanged = (width != framebufferWidth) || (height != framebufferHeight);
    framebufferWidth = width;
    framebufferHeight = height;

    if (waylandWindow && sizeChanged) {
        waylandWindow->width = static_cast<uint32_t>(std::max(1, framebufferWidth));
        waylandWindow->height = static_cast<uint32_t>(std::max(1, framebufferHeight));
    }
    if (engine && swapChain && sizeChanged) {
        engine->destroy(swapChain);
        if (swapChainIsNative && nativeSwapChainHandle) {
            swapChain = engine->createSwapChain(nativeSwapChainHandle);
        } else {
            swapChain = engine->createSwapChain(static_cast<uint32_t>(framebufferWidth),
                                                static_cast<uint32_t>(framebufferHeight));
        }
    }

    for (auto& [layer, state] : layers) {
        if (state.view) {
            filament::Viewport viewport{0, 0, static_cast<uint32_t>(framebufferWidth), static_cast<uint32_t>(framebufferHeight)};
            state.view->setViewport(viewport);
        }
    }

    if (!lastProjectionWasOrtho) {
        setPerspective(lastFovDegrees, lastAspect, lastNear, lastFar);
    } else {
        setOrthographic(lastOrthoLeft, lastOrthoRight, lastOrthoTop, lastOrthoBottom, lastNear, lastFar);
    }
}

void FilamentBackend::ensureLayer(graphics::LayerId layer) {
    if (!engine) {
        return;
    }
    if (layers.find(layer) != layers.end()) {
        return;
    }

    LayerState state;
    state.scene = engine->createScene();
    state.view = engine->createView();
    if (state.view) {
        filament::Viewport viewport{0, 0, static_cast<uint32_t>(framebufferWidth), static_cast<uint32_t>(framebufferHeight)};
        state.view->setViewport(viewport);
        state.view->setCamera(camera);
        state.view->setScene(state.scene);
        if (renderer) {
            filament::Renderer::ClearOptions clearOptions{};
            clearOptions.clear = true;
            clearOptions.discard = true;
            clearOptions.clearColor = filament::math::float4{0.05f, 0.08f, 0.12f, 1.0f};
            renderer->setClearOptions(clearOptions);
        }
    }
    layers[layer] = state;
    ensureSceneLighting(layer);
}

void FilamentBackend::applyTransform(const EntityRecord& record) {
    if (!engine) {
        return;
    }

    auto& tm = engine->getTransformManager();
    auto inst = tm.getInstance(record.entity);
    if (!inst.isValid()) {
        return;
    }

    glm::mat4 transform(1.0f);
    transform = glm::translate(transform, record.position);
    transform *= glm::mat4_cast(record.rotation);
    transform = glm::scale(transform, record.scale);

    tm.setTransform(inst, toFilamentMat4(transform));
}

void FilamentBackend::updateViewMatrix() {
    glm::mat4 world(1.0f);
    world = glm::translate(world, cameraPosition);
    world *= glm::mat4_cast(cameraRotation);
    viewMatrix = glm::inverse(world);
    if (camera) {
        camera->setModelMatrix(toFilamentMat4(world));
    }
}

void FilamentBackend::updateProjectionMatrix() {
    if (camera) {
        if (lastProjectionWasOrtho) {
            camera->setProjection(filament::Camera::Projection::ORTHO,
                                  lastOrthoLeft, lastOrthoRight,
                                  lastOrthoBottom, lastOrthoTop,
                                  lastNear, lastFar);
        } else {
            camera->setProjection(lastFovDegrees, lastAspect, lastNear, lastFar, filament::Camera::Fov::VERTICAL);
        }
    }

    if (lastProjectionWasOrtho) {
        projectionMatrix = glm::ortho(lastOrthoLeft, lastOrthoRight, lastOrthoBottom, lastOrthoTop, lastNear, lastFar);
    } else {
        projectionMatrix = glm::perspective(glm::radians(lastFovDegrees), lastAspect, lastNear, lastFar);
    }
}

void FilamentBackend::ensureSceneLighting(graphics::LayerId layer) {
    if (!engine) {
        return;
    }

    IblConfig lightingConfig{};
    if (!lightInitialized) {
        lightingConfig = resolveIblConfig();
        lightEntity = utils::EntityManager::get().create();
        filament::LightManager::Builder(filament::LightManager::Type::DIRECTIONAL)
            .color(lightingConfig.keyLightColor)
            .intensity(lightingConfig.keyLightIntensity)
            .direction({0.2f, -1.0f, -0.2f})
            .castShadows(false)
            .build(*engine, lightEntity);

        ambientEntity = utils::EntityManager::get().create();
        filament::LightManager::Builder(filament::LightManager::Type::DIRECTIONAL)
            .color(lightingConfig.fillLightColor)
            .intensity(lightingConfig.fillLightIntensity)
            .direction({-0.2f, -1.0f, 0.2f})
            .castShadows(false)
            .build(*engine, ambientEntity);

        lightInitialized = true;
        keyLightBaseIntensity = lightingConfig.keyLightIntensity;
        fillLightBaseIntensity = lightingConfig.fillLightIntensity;
        layers[layer].scene->addEntity(ambientEntity);
    }

    auto it = layers.find(layer);
    if (it != layers.end() && it->second.scene) {
        it->second.scene->addEntity(lightEntity);
        if (ambientEntity) {
            it->second.scene->addEntity(ambientEntity);
        }
        if (!indirectLight) {
            if (!iblInitialized) {
                if (!lightInitialized) {
                    lightingConfig = resolveIblConfig();
                }
                if (lightingConfig.hasIbl) {
                    iblTexture = loadKtx1Texture(engine, lightingConfig.iblPath, false);
                    if (iblTexture) {
                        indirectLight = filament::IndirectLight::Builder()
                            .reflections(iblTexture)
                            .intensity(lightingConfig.intensity)
                            .build(*engine);
                        iblBaseIntensity = lightingConfig.intensity;
                    }
                }
                if (lightingConfig.hasSkybox) {
                    skyboxTexture = loadKtx1Texture(engine, lightingConfig.skyboxPath, true);
                    if (skyboxTexture) {
                        skybox = filament::Skybox::Builder()
                            .environment(skyboxTexture)
                            .build(*engine);
                    }
                } else if (lightingConfig.hasSkyboxColor) {
                    skybox = filament::Skybox::Builder()
                        .color(lightingConfig.skyboxColor)
                        .build(*engine);
                }
                iblInitialized = true;
            }
        }
        if (indirectLight) {
            it->second.scene->setIndirectLight(indirectLight);
        }
        if (skybox) {
            it->second.scene->setSkybox(skybox);
        }
    }
}

void FilamentBackend::destroyAsset(graphics::EntityId id) {
    auto it = assets.find(id);
    if (it == assets.end()) {
        return;
    }

    auto* asset = it->second;
    if (asset) {
        auto recordIt = entities.find(id);
        if (recordIt != entities.end()) {
            auto layerIt = layers.find(recordIt->second.layer);
            if (layerIt != layers.end() && layerIt->second.scene) {
                layerIt->second.scene->removeEntities(asset->getEntities(), asset->getEntityCount());
            }
        }
        if (assetLoader) {
            assetLoader->destroyAsset(asset);
        }
    }

    assets.erase(it);
}

graphics::EntityId FilamentBackend::createEntity(graphics::LayerId layer) {
    if (!engine) {
        return graphics::kInvalidEntity;
    }

    ensureLayer(layer);
    utils::Entity entity = utils::EntityManager::get().create();
    engine->getTransformManager().create(entity);
    if (layers[layer].scene) {
        layers[layer].scene->addEntity(entity);
    }

    const graphics::EntityId id = nextEntityId++;
    EntityRecord record;
    record.layer = layer;
    record.entity = entity;
    entities[id] = record;
    applyTransform(record);
    return id;
}

graphics::EntityId FilamentBackend::createModelEntity(const std::filesystem::path& modelPath,
                                                      graphics::LayerId layer,
                                                      graphics::MaterialId materialOverride) {
    const graphics::EntityId id = createEntity(layer);
    setEntityModel(id, modelPath, materialOverride);
    return id;
}

graphics::EntityId FilamentBackend::createMeshEntity(graphics::MeshId mesh,
                                                     graphics::LayerId layer,
                                                     graphics::MaterialId materialOverride) {
    const graphics::EntityId id = createEntity(layer);
    setEntityMesh(id, mesh, materialOverride);
    return id;
}

void FilamentBackend::setEntityModel(graphics::EntityId entity,
                                     const std::filesystem::path& modelPath,
                                     graphics::MaterialId) {
    if (!engine || !assetLoader) {
        return;
    }

    auto recordIt = entities.find(entity);
    if (recordIt == entities.end()) {
        return;
    }

    if (modelPath.empty()) {
        spdlog::warn("Graphics(Filament): empty model path for entity {}", entity);
        return;
    }
    destroyAsset(entity);
    modelPaths[entity] = modelPath;

    const std::vector<uint8_t> blob = readFileToBytes(modelPath);
    if (blob.empty()) {
        spdlog::error("Graphics(Filament): Empty model file '{}'", modelPath.string());
        return;
    }

    filament::gltfio::FilamentAsset* asset = assetLoader->createAsset(blob.data(), static_cast<uint32_t>(blob.size()));
    if (!asset) {
        spdlog::error("Graphics(Filament): Failed to load model '{}'", modelPath.string());
        return;
    }

    const std::string baseDir = modelPath.parent_path().string();
    filament::gltfio::ResourceConfiguration config{};
    config.engine = engine;
    config.gltfPath = baseDir.c_str();
    config.normalizeSkinningWeights = true;
    filament::gltfio::ResourceLoader resourceLoader(config);
    if (textureProvider) {
        resourceLoader.addTextureProvider("image/png", textureProvider);
        resourceLoader.addTextureProvider("image/jpeg", textureProvider);
    }

    if (!resourceLoader.loadResources(asset)) {
        spdlog::warn("Graphics(Filament): Failed to load resources for '{}'", modelPath.string());
    }
    asset->releaseSourceData();

    ensureLayer(recordIt->second.layer);
    auto& state = layers[recordIt->second.layer];
    if (state.scene) {
        state.scene->addEntities(asset->getEntities(), asset->getEntityCount());
    }

    auto& tm = engine->getTransformManager();
    auto anchorInstance = tm.getInstance(recordIt->second.entity);
    auto assetRoot = asset->getRoot();
    auto assetRootInstance = tm.getInstance(assetRoot);
    if (anchorInstance.isValid() && assetRootInstance.isValid()) {
        tm.setParent(assetRootInstance, anchorInstance);
    }

    assets[entity] = asset;
}

void FilamentBackend::setEntityMesh(graphics::EntityId entity,
                                    graphics::MeshId,
                                    graphics::MaterialId) {
    if (!warnedMissingTargets) {
        spdlog::warn("Graphics(Filament): mesh rendering not implemented yet; entity {} is a placeholder", entity);
        warnedMissingTargets = true;
    }
    (void)entity;
}

void FilamentBackend::destroyEntity(graphics::EntityId entity) {
    auto it = entities.find(entity);
    if (it == entities.end() || !engine) {
        return;
    }

    destroyAsset(entity);

    auto& record = it->second;
    auto layerIt = layers.find(record.layer);
    if (layerIt != layers.end() && layerIt->second.scene) {
        layerIt->second.scene->remove(record.entity);
    }

    auto& tm = engine->getTransformManager();
    auto inst = tm.getInstance(record.entity);
    if (inst.isValid()) {
        tm.destroy(record.entity);
    }
    utils::EntityManager::get().destroy(record.entity);
    entities.erase(it);
    modelPaths.erase(entity);
}

graphics::MeshId FilamentBackend::createMesh(const graphics::MeshData& mesh) {
    const graphics::MeshId id = nextMeshId++;
    meshes[id] = mesh;
    return id;
}

void FilamentBackend::destroyMesh(graphics::MeshId mesh) {
    meshes.erase(mesh);
}

graphics::MaterialId FilamentBackend::createMaterial(const graphics::MaterialDesc& material) {
    const graphics::MaterialId id = nextMaterialId++;
    materials[id] = material;
    return id;
}

void FilamentBackend::updateMaterial(graphics::MaterialId material, const graphics::MaterialDesc& desc) {
    materials[material] = desc;
}

void FilamentBackend::destroyMaterial(graphics::MaterialId material) {
    materials.erase(material);
}

void FilamentBackend::setMaterialFloat(graphics::MaterialId, std::string_view, float) {
    // TODO(filament): apply material parameters once material system is wired.
}

graphics::RenderTargetId FilamentBackend::createRenderTarget(const graphics::RenderTargetDesc& desc) {
    if (!engine) {
        return graphics::kDefaultRenderTarget;
    }

    const graphics::RenderTargetId id = nextTargetId++;
    RenderTargetRecord record;
    record.desc = desc;

    const int width = std::max(1, desc.width);
    const int height = std::max(1, desc.height);

    record.colorTexture = filament::Texture::Builder()
        .width(static_cast<uint32_t>(width))
        .height(static_cast<uint32_t>(height))
        .levels(1)
        .sampler(filament::Texture::Sampler::SAMPLER_2D)
        .format(filament::Texture::InternalFormat::RGBA8)
        .usage(filament::Texture::Usage::COLOR_ATTACHMENT | filament::Texture::Usage::SAMPLEABLE)
        .build(*engine);

    if (desc.depth) {
        const auto depthFormat = desc.stencil
            ? filament::Texture::InternalFormat::DEPTH24_STENCIL8
            : filament::Texture::InternalFormat::DEPTH24;
        record.depthTexture = filament::Texture::Builder()
            .width(static_cast<uint32_t>(width))
            .height(static_cast<uint32_t>(height))
            .levels(1)
            .sampler(filament::Texture::Sampler::SAMPLER_2D)
            .format(depthFormat)
            .usage(filament::Texture::Usage::DEPTH_ATTACHMENT | filament::Texture::Usage::SAMPLEABLE)
            .build(*engine);
    }

    filament::RenderTarget::Builder rtBuilder;
    rtBuilder.texture(filament::RenderTarget::AttachmentPoint::COLOR, record.colorTexture);
    if (record.depthTexture) {
        rtBuilder.texture(filament::RenderTarget::AttachmentPoint::DEPTH, record.depthTexture);
    }
    record.target = rtBuilder.build(*engine);

    renderTargets[id] = record;
    return id;
}

void FilamentBackend::destroyRenderTarget(graphics::RenderTargetId target) {
    auto it = renderTargets.find(target);
    if (it == renderTargets.end() || !engine) {
        return;
    }

    auto& record = it->second;
    if (record.target) {
        engine->destroy(record.target);
        record.target = nullptr;
    }
    if (record.colorTexture) {
        engine->destroy(record.colorTexture);
        record.colorTexture = nullptr;
    }
    if (record.depthTexture) {
        engine->destroy(record.depthTexture);
        record.depthTexture = nullptr;
    }
    renderTargets.erase(it);
}

void FilamentBackend::renderLayer(graphics::LayerId layer, graphics::RenderTargetId target) {
    if (!engine || !renderer) {
        return;
    }

    ensureLayer(layer);

    auto& state = layers[layer];
    if (state.view && state.scene) {
        if (target == graphics::kDefaultRenderTarget) {
            state.view->setRenderTarget(nullptr);
        } else {
            auto rtIt = renderTargets.find(target);
            if (rtIt != renderTargets.end()) {
                state.view->setRenderTarget(rtIt->second.target);
            }
        }
        state.view->setCamera(camera);
        state.view->setScene(state.scene);
        if (frameActive) {
            renderer->render(state.view);
        }
    }
}

unsigned int FilamentBackend::getRenderTargetTextureId(graphics::RenderTargetId target) const {
    auto it = renderTargets.find(target);
    if (it == renderTargets.end()) {
        return 0u;
    }
    return it->second.colorTextureId;
}

void FilamentBackend::setUiOverlayTexture(const graphics::TextureHandle& texture) {
    if (!engine || !uiMaterialInstance) {
        return;
    }

    if (!texture.valid()) {
        uiTextureId = 0;
        uiTextureWidth = 0;
        uiTextureHeight = 0;
        if (uiTexture) {
            engine->destroy(uiTexture);
            uiTexture = nullptr;
        }
        return;
    }

    const auto textureId = static_cast<unsigned int>(texture.id);
    const int width = static_cast<int>(texture.width);
    const int height = static_cast<int>(texture.height);
    if (textureId == uiTextureId && width == uiTextureWidth && height == uiTextureHeight) {
        return;
    }
    uiTextureId = textureId;
    uiTextureWidth = width;
    uiTextureHeight = height;

    if (uiTexture) {
        engine->destroy(uiTexture);
        uiTexture = nullptr;
    }

    uiTexture = filament::Texture::Builder()
        .width(uiTextureWidth)
        .height(uiTextureHeight)
        .levels(1)
        .sampler(filament::Texture::Sampler::SAMPLER_2D)
        .format(filament::Texture::InternalFormat::RGBA8)
        .usage(filament::Texture::Usage::SAMPLEABLE)
        .import(textureId)
        .build(*engine);

    if (uiTexture) {
        uiMaterialInstance->setParameter("baseColorMap", uiTexture, uiSampler);
        uiMaterialInstance->setParameter("baseColorFactor", filament::math::float4{1.0f, 1.0f, 1.0f, 1.0f});
    }
}

void FilamentBackend::setUiOverlayVisible(bool visible) {
    uiVisible = visible;
    if (!uiScene || !uiQuadEntity) {
        return;
    }
    if (visible && !uiInScene) {
        uiScene->addEntity(uiQuadEntity);
        uiInScene = true;
    } else if (!visible && uiInScene) {
        uiScene->remove(uiQuadEntity);
        uiInScene = false;
    }
}

void FilamentBackend::renderUiOverlay() {
    if (!renderer || !frameActive || !uiView || !uiScene || !uiMaterialInstance || !uiTexture || !uiVisible) {
        return;
    }

    if (uiView) {
        filament::Viewport viewport{0, 0, static_cast<uint32_t>(framebufferWidth), static_cast<uint32_t>(framebufferHeight)};
        uiView->setViewport(viewport);
    }

    const auto prevClear = renderer->getClearOptions();
    filament::Renderer::ClearOptions clearOptions = prevClear;
    clearOptions.clear = false;
    renderer->setClearOptions(clearOptions);
    renderer->render(uiView);
    renderer->setClearOptions(prevClear);
}

void FilamentBackend::setBrightness(float value) {
    if (std::abs(value - brightness) < 0.0001f) {
        return;
    }
    brightness = value;

    if (engine) {
        auto& lm = engine->getLightManager();
        if (lightEntity) {
            auto inst = lm.getInstance(lightEntity);
            if (inst.isValid()) {
                lm.setIntensity(inst, keyLightBaseIntensity * brightness);
            }
        }
        if (ambientEntity) {
            auto inst = lm.getInstance(ambientEntity);
            if (inst.isValid()) {
                lm.setIntensity(inst, fillLightBaseIntensity * brightness);
            }
        }
    }
    if (indirectLight) {
        indirectLight->setIntensity(iblBaseIntensity * brightness);
    }
}

void FilamentBackend::setPosition(graphics::EntityId entity, const glm::vec3& position) {
    auto it = entities.find(entity);
    if (it == entities.end()) {
        return;
    }
    it->second.position = position;
    applyTransform(it->second);
}

void FilamentBackend::setRotation(graphics::EntityId entity, const glm::quat& rotation) {
    auto it = entities.find(entity);
    if (it == entities.end()) {
        return;
    }
    it->second.rotation = rotation;
    applyTransform(it->second);
}

void FilamentBackend::setScale(graphics::EntityId entity, const glm::vec3& scale) {
    auto it = entities.find(entity);
    if (it == entities.end()) {
        return;
    }
    it->second.scale = scale;
    applyTransform(it->second);
}

void FilamentBackend::setVisible(graphics::EntityId entity, bool visible) {
    auto it = entities.find(entity);
    if (it == entities.end()) {
        return;
    }
    it->second.visible = visible;
    // TODO(filament): wire visibility into renderable manager.
}

void FilamentBackend::setTransparency(graphics::EntityId, bool) {
    // TODO(filament): update material transparency once materials are wired.
}

void FilamentBackend::setCameraPosition(const glm::vec3& position) {
    cameraPosition = position;
    updateViewMatrix();
}

void FilamentBackend::setCameraRotation(const glm::quat& rotation) {
    cameraRotation = rotation;
    updateViewMatrix();
}

void FilamentBackend::setPerspective(float fovDegrees, float aspect, float nearPlane, float farPlane) {
    lastFovDegrees = fovDegrees;
    lastAspect = aspect;
    lastNear = nearPlane;
    lastFar = farPlane;
    lastProjectionWasOrtho = false;
    updateProjectionMatrix();
}

void FilamentBackend::setOrthographic(float left, float right, float top, float bottom, float nearPlane, float farPlane) {
    lastOrthoLeft = left;
    lastOrthoRight = right;
    lastOrthoTop = top;
    lastOrthoBottom = bottom;
    lastNear = nearPlane;
    lastFar = farPlane;
    lastProjectionWasOrtho = true;
    updateProjectionMatrix();
}

glm::mat4 FilamentBackend::getViewProjectionMatrix() const {
    return projectionMatrix * viewMatrix;
}

glm::mat4 FilamentBackend::getViewMatrix() const {
    return viewMatrix;
}

glm::mat4 FilamentBackend::getProjectionMatrix() const {
    return projectionMatrix;
}

glm::vec3 FilamentBackend::getCameraPosition() const {
    return cameraPosition;
}

glm::vec3 FilamentBackend::getCameraForward() const {
    return cameraRotation * glm::vec3(0.0f, 0.0f, -1.0f);
}

} // namespace graphics_backend
