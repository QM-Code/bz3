#include "ui/backends/imgui/backend.hpp"

#if defined(BZ3_RENDER_BACKEND_BGFX)
#include <bgfx/bgfx.h>
#include <bgfx/platform.h>
#include <bx/math.h>
#else
#include <imgui_impl_opengl3.h>
#endif

#include "common/data_path_resolver.hpp"
#include "common/config_helpers.hpp"
#include "common/i18n.hpp"
#include "platform/window.hpp"
#include "spdlog/spdlog.h"

#include <algorithm>
#include <cstring>
#include <fstream>

namespace ui_backend {
namespace {

ImGuiKey toImGuiKey(platform::Key key) {
    switch (key) {
        case platform::Key::A: return ImGuiKey_A;
        case platform::Key::B: return ImGuiKey_B;
        case platform::Key::C: return ImGuiKey_C;
        case platform::Key::D: return ImGuiKey_D;
        case platform::Key::E: return ImGuiKey_E;
        case platform::Key::F: return ImGuiKey_F;
        case platform::Key::G: return ImGuiKey_G;
        case platform::Key::H: return ImGuiKey_H;
        case platform::Key::I: return ImGuiKey_I;
        case platform::Key::J: return ImGuiKey_J;
        case platform::Key::K: return ImGuiKey_K;
        case platform::Key::L: return ImGuiKey_L;
        case platform::Key::M: return ImGuiKey_M;
        case platform::Key::N: return ImGuiKey_N;
        case platform::Key::O: return ImGuiKey_O;
        case platform::Key::P: return ImGuiKey_P;
        case platform::Key::Q: return ImGuiKey_Q;
        case platform::Key::R: return ImGuiKey_R;
        case platform::Key::S: return ImGuiKey_S;
        case platform::Key::T: return ImGuiKey_T;
        case platform::Key::U: return ImGuiKey_U;
        case platform::Key::V: return ImGuiKey_V;
        case platform::Key::W: return ImGuiKey_W;
        case platform::Key::X: return ImGuiKey_X;
        case platform::Key::Y: return ImGuiKey_Y;
        case platform::Key::Z: return ImGuiKey_Z;
        case platform::Key::Num0: return ImGuiKey_0;
        case platform::Key::Num1: return ImGuiKey_1;
        case platform::Key::Num2: return ImGuiKey_2;
        case platform::Key::Num3: return ImGuiKey_3;
        case platform::Key::Num4: return ImGuiKey_4;
        case platform::Key::Num5: return ImGuiKey_5;
        case platform::Key::Num6: return ImGuiKey_6;
        case platform::Key::Num7: return ImGuiKey_7;
        case platform::Key::Num8: return ImGuiKey_8;
        case platform::Key::Num9: return ImGuiKey_9;
        case platform::Key::F1: return ImGuiKey_F1;
        case platform::Key::F2: return ImGuiKey_F2;
        case platform::Key::F3: return ImGuiKey_F3;
        case platform::Key::F4: return ImGuiKey_F4;
        case platform::Key::F5: return ImGuiKey_F5;
        case platform::Key::F6: return ImGuiKey_F6;
        case platform::Key::F7: return ImGuiKey_F7;
        case platform::Key::F8: return ImGuiKey_F8;
        case platform::Key::F9: return ImGuiKey_F9;
        case platform::Key::F10: return ImGuiKey_F10;
        case platform::Key::F11: return ImGuiKey_F11;
        case platform::Key::F12: return ImGuiKey_F12;
        case platform::Key::F13: return ImGuiKey_F13;
        case platform::Key::F14: return ImGuiKey_F14;
        case platform::Key::F15: return ImGuiKey_F15;
        case platform::Key::F16: return ImGuiKey_F16;
        case platform::Key::F17: return ImGuiKey_F17;
        case platform::Key::F18: return ImGuiKey_F18;
        case platform::Key::F19: return ImGuiKey_F19;
        case platform::Key::F20: return ImGuiKey_F20;
        case platform::Key::F21: return ImGuiKey_F21;
        case platform::Key::F22: return ImGuiKey_F22;
        case platform::Key::F23: return ImGuiKey_F23;
        case platform::Key::F24: return ImGuiKey_F24;
        case platform::Key::Space: return ImGuiKey_Space;
        case platform::Key::Escape: return ImGuiKey_Escape;
        case platform::Key::Enter: return ImGuiKey_Enter;
        case platform::Key::Tab: return ImGuiKey_Tab;
        case platform::Key::Backspace: return ImGuiKey_Backspace;
        case platform::Key::Left: return ImGuiKey_LeftArrow;
        case platform::Key::Right: return ImGuiKey_RightArrow;
        case platform::Key::Up: return ImGuiKey_UpArrow;
        case platform::Key::Down: return ImGuiKey_DownArrow;
        case platform::Key::LeftBracket: return ImGuiKey_LeftBracket;
        case platform::Key::RightBracket: return ImGuiKey_RightBracket;
        case platform::Key::Minus: return ImGuiKey_Minus;
        case platform::Key::Equal: return ImGuiKey_Equal;
        case platform::Key::Apostrophe: return ImGuiKey_Apostrophe;
        case platform::Key::GraveAccent: return ImGuiKey_GraveAccent;
        case platform::Key::LeftShift: return ImGuiKey_LeftShift;
        case platform::Key::RightShift: return ImGuiKey_RightShift;
        case platform::Key::LeftControl: return ImGuiKey_LeftCtrl;
        case platform::Key::RightControl: return ImGuiKey_RightCtrl;
        case platform::Key::LeftAlt: return ImGuiKey_LeftAlt;
        case platform::Key::RightAlt: return ImGuiKey_RightAlt;
        case platform::Key::LeftSuper: return ImGuiKey_LeftSuper;
        case platform::Key::RightSuper: return ImGuiKey_RightSuper;
        case platform::Key::Menu: return ImGuiKey_Menu;
        case platform::Key::Home: return ImGuiKey_Home;
        case platform::Key::End: return ImGuiKey_End;
        case platform::Key::PageUp: return ImGuiKey_PageUp;
        case platform::Key::PageDown: return ImGuiKey_PageDown;
        case platform::Key::Insert: return ImGuiKey_Insert;
        case platform::Key::Delete: return ImGuiKey_Delete;
        default: return ImGuiKey_None;
    }
}

int toImGuiMouseButton(platform::MouseButton button) {
    switch (button) {
        case platform::MouseButton::Left: return 0;
        case platform::MouseButton::Right: return 1;
        case platform::MouseButton::Middle: return 2;
        case platform::MouseButton::Button4: return 3;
        case platform::MouseButton::Button5: return 4;
        case platform::MouseButton::Button6: return 5;
        case platform::MouseButton::Button7: return 6;
        case platform::MouseButton::Button8: return 7;
        default: return 0;
    }
}

void UpdateModifiers(ImGuiIO &io, platform::Window *window) {
    if (!window) {
        return;
    }
    const bool shift = window->isKeyDown(platform::Key::LeftShift) || window->isKeyDown(platform::Key::RightShift);
    const bool ctrl = window->isKeyDown(platform::Key::LeftControl) || window->isKeyDown(platform::Key::RightControl);
    const bool alt = window->isKeyDown(platform::Key::LeftAlt) || window->isKeyDown(platform::Key::RightAlt);
    const bool super = window->isKeyDown(platform::Key::LeftSuper) || window->isKeyDown(platform::Key::RightSuper);
    io.AddKeyEvent(ImGuiKey_ModShift, shift);
    io.AddKeyEvent(ImGuiKey_ModCtrl, ctrl);
    io.AddKeyEvent(ImGuiKey_ModAlt, alt);
    io.AddKeyEvent(ImGuiKey_ModSuper, super);
}

const char *GetClipboardText(void *user_data) {
    static std::string buffer;
    auto *window = static_cast<platform::Window *>(user_data);
    buffer = window ? window->getClipboardText() : std::string();
    return buffer.c_str();
}

void SetClipboardText(void *user_data, const char *text) {
    auto *window = static_cast<platform::Window *>(user_data);
    if (window) {
        window->setClipboardText(text ? text : "");
    }
}

} // namespace

ImGuiBackend::ImGuiBackend(platform::Window &windowRef) : window(&windowRef) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO &io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.BackendPlatformName = "bz3-platform";
    io.SetClipboardTextFn = SetClipboardText;
    io.GetClipboardTextFn = GetClipboardText;
    io.ClipboardUserData = window;

    ImGui::StyleColorsDark();

#if defined(BZ3_RENDER_BACKEND_BGFX)
    io.BackendRendererName = "bz3-imgui-bgfx";
    spdlog::info("UiSystem: ImGui bgfx renderer init start");
    initBgfxRenderer();
#else
    ImGui_ImplOpenGL3_Init("#version 330");
#endif

    spdlog::info("UiSystem: ImGui add default font");
    io.Fonts->AddFontDefault();

    const auto bigFontPath = bz::data::ResolveConfiguredAsset("hud.fonts.console.Regular.Font");
    const std::string bigFontPathStr = bigFontPath.string();
    spdlog::info("UiSystem: ImGui add big font from {}", bigFontPathStr);
    bigFont = io.Fonts->AddFontFromFileTTF(
        bigFontPathStr.c_str(),
        100.0f
    );

    if (!bigFont) {
        spdlog::warn("UiSystem: Failed to load font at {}", bigFontPathStr);
    }

    spdlog::info("UiSystem: ImGui console font init start");
    consoleView.initializeFonts(io);
    spdlog::info("UiSystem: ImGui console font init done");

    showFPS = bz::data::ReadBoolConfig({"debug.ShowFPS"}, false);
    hud.setShowFps(showFPS);

    consoleView.setLanguageCallback([this](const std::string &language) {
#if defined(BZ3_RENDER_BACKEND_BGFX)
        pendingLanguage = language;
        languageReloadArmed = true;
#else
        pendingLanguage = language;
        languageReloadArmed = true;
#endif
    });

    spdlog::info("UiSystem: ImGui font atlas build start");
#if defined(BZ3_RENDER_BACKEND_BGFX)
    io.Fonts->Build();
    spdlog::info("UiSystem: ImGui font atlas build done");
    spdlog::info("UiSystem: ImGui bgfx font build start");
    buildBgfxFonts();
    io.FontDefault = io.Fonts->Fonts.empty() ? nullptr : io.Fonts->Fonts[0];
#else
    io.Fonts->Build();
    spdlog::info("UiSystem: ImGui font atlas build done");
#endif
}

ImGuiBackend::~ImGuiBackend() {
#if defined(BZ3_RENDER_BACKEND_BGFX)
    shutdownBgfxRenderer();
#else
    ImGui_ImplOpenGL3_Shutdown();
#endif
    ImGui::DestroyContext();
}

void ImGuiBackend::handleEvents(const std::vector<platform::Event> &events) {
    ImGuiIO &io = ImGui::GetIO();
    for (const auto &event : events) {
        switch (event.type) {
            case platform::EventType::KeyDown:
            case platform::EventType::KeyUp: {
                const bool down = (event.type == platform::EventType::KeyDown);
                const ImGuiKey key = toImGuiKey(event.key);
                if (key != ImGuiKey_None) {
                    io.AddKeyEvent(key, down);
                }
                break;
            }
            case platform::EventType::TextInput: {
                if (event.codepoint != 0) {
                    io.AddInputCharacter(static_cast<unsigned int>(event.codepoint));
                }
                break;
            }
            case platform::EventType::MouseButtonDown:
            case platform::EventType::MouseButtonUp: {
                const bool down = (event.type == platform::EventType::MouseButtonDown);
                io.AddMouseButtonEvent(toImGuiMouseButton(event.mouseButton), down);
                break;
            }
            case platform::EventType::MouseMove: {
                io.AddMousePosEvent(static_cast<float>(event.x), static_cast<float>(event.y));
                break;
            }
            case platform::EventType::MouseScroll: {
                io.AddMouseWheelEvent(static_cast<float>(event.scrollX), static_cast<float>(event.scrollY));
                break;
            }
            case platform::EventType::WindowFocus: {
                io.AddFocusEvent(event.focused);
                break;
            }
            default:
                break;
        }
    }
}

void ImGuiBackend::update() {
    if (languageReloadArmed && pendingLanguage) {
        languageReloadArmed = false;
        bz::i18n::Get().loadLanguage(*pendingLanguage);
        pendingLanguage.reset();
        reloadFonts();
    }
    if (consoleView.consumeFontReloadRequest()) {
        reloadFonts();
    }
    if (renderBridge) {
        hud.setRadarTexture(renderBridge->getRadarTexture());
    }

    ImGuiIO &io = ImGui::GetIO();

    const auto now = std::chrono::steady_clock::now();
    if (lastFrameTime.time_since_epoch().count() == 0) {
        io.DeltaTime = 1.0f / 60.0f;
    } else {
        const std::chrono::duration<float> dt = now - lastFrameTime;
        io.DeltaTime = dt.count();
    }
    lastFrameTime = now;

    int fbWidth = 0;
    int fbHeight = 0;
    if (window) {
        window->getFramebufferSize(fbWidth, fbHeight);
    }
    io.DisplaySize = ImVec2(static_cast<float>(fbWidth), static_cast<float>(fbHeight));
    io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);

    UpdateModifiers(io, window);
    if (window) {
        window->setCursorVisible(!io.MouseDrawCursor);
    }

#if defined(BZ3_RENDER_BACKEND_BGFX)
    ImGuiIO &ioRef = ImGui::GetIO();
    ioRef.BackendRendererName = "bz3-imgui-bgfx";
#else
    ImGui_ImplOpenGL3_NewFrame();
#endif
    io.FontGlobalScale = 1.0f;
    ImGui::NewFrame();

    if (consoleView.isVisible()) {
        consoleView.draw(io);
    } else {
        hud.setShowFps(showFPS);
        hud.draw(io, bigFont);
    }

#if defined(BZ3_RENDER_BACKEND_BGFX)
    if (!imguiFontsReady) {
        ImGui::EndFrame();
        return;
    }
#endif

    ImGui::Render();
#if defined(BZ3_RENDER_BACKEND_BGFX)
    renderBgfxDrawData(ImGui::GetDrawData());
#else
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
#endif
}

void ImGuiBackend::reloadFonts() {
    ImGuiIO &io = ImGui::GetIO();
    io.Fonts->Clear();
    io.Fonts->AddFontDefault();

    const auto bigFontPath = bz::data::ResolveConfiguredAsset("hud.fonts.console.Regular.Font");
    const std::string bigFontPathStr = bigFontPath.string();
    bigFont = io.Fonts->AddFontFromFileTTF(
        bigFontPathStr.c_str(),
        100.0f
    );

    if (!bigFont) {
        spdlog::warn("UiSystem: Failed to load font at {}", bigFontPathStr);
    }

    consoleView.initializeFonts(io);
    io.Fonts->Build();
#if defined(BZ3_RENDER_BACKEND_BGFX)
    buildBgfxFonts();
    io.FontDefault = io.Fonts->Fonts.empty() ? nullptr : io.Fonts->Fonts[0];
#else
    ImGui_ImplOpenGL3_DestroyFontsTexture();
    ImGui_ImplOpenGL3_CreateFontsTexture();
#endif
}

void ImGuiBackend::setScoreboardEntries(const std::vector<ScoreboardEntry> &entries) {
    hud.setScoreboardEntries(entries);
}

void ImGuiBackend::setSpawnHint(const std::string &hint) {
    hud.setSpawnHint(hint);
}

void ImGuiBackend::addConsoleLine(const std::string &playerName, const std::string &line) {
    hud.addConsoleLine(playerName, line);
}

std::string ImGuiBackend::getChatInputBuffer() const {
    return hud.getChatInputBuffer();
}

void ImGuiBackend::clearChatInputBuffer() {
    hud.clearChatInputBuffer();
}

void ImGuiBackend::focusChatInput() {
    hud.focusChatInput();
}

bool ImGuiBackend::getChatInputFocus() const {
    return hud.getChatInputFocus();
}

void ImGuiBackend::displayDeathScreen(bool show) {
    hud.displayDeathScreen(show);
}

bool ImGuiBackend::consumeKeybindingsReloadRequest() {
    return consoleView.consumeKeybindingsReloadRequest();
}

void ImGuiBackend::setRenderBridge(const ui::RenderBridge *bridge) {
    renderBridge = bridge;
}

ui::RenderOutput ImGuiBackend::getRenderOutput() const {
    return {};
}

ui::ConsoleInterface &ImGuiBackend::console() {
    return consoleView;
}

const ui::ConsoleInterface &ImGuiBackend::console() const {
    return consoleView;
}

#if defined(BZ3_RENDER_BACKEND_BGFX)
namespace {

struct ImGuiVertex {
    float x;
    float y;
    float u;
    float v;
    uint32_t abgr;
};

static uint16_t ToTextureHandle(ImTextureID textureId) {
    if (textureId == 0) {
        return bgfx::kInvalidHandle;
    }
    const uint64_t value = static_cast<uint64_t>(textureId);
    if (value == 0) {
        return bgfx::kInvalidHandle;
    }
    return static_cast<uint16_t>(value - 1);
}

} // namespace

void ImGuiBackend::initBgfxRenderer() {
    imguiTexture = bgfx::createUniform("s_tex", bgfx::UniformType::Sampler);
    imguiScaleBias = bgfx::createUniform("u_scaleBias", bgfx::UniformType::Vec4);

    const std::filesystem::path shaderDir = []() {
        std::filesystem::path base = bz::data::Resolve("bgfx/shaders/bin");
        const bgfx::RendererType::Enum renderer = bgfx::getRendererType();
        switch (renderer) {
            case bgfx::RendererType::OpenGL:
            case bgfx::RendererType::OpenGLES:
                base /= "gl";
                break;
            default:
                base /= "vk";
                break;
        }
        return base / "imgui";
    }();
    const auto vsPath = shaderDir / "vs_imgui.bin";
    const auto fsPath = shaderDir / "fs_imgui.bin";

    auto readBytes = [](const std::filesystem::path& path) -> std::vector<uint8_t> {
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
    };

    auto vsBytes = readBytes(vsPath);
    auto fsBytes = readBytes(fsPath);
    if (vsBytes.empty() || fsBytes.empty()) {
        spdlog::error("UiSystem: missing ImGui bgfx shaders '{}', '{}'", vsPath.string(), fsPath.string());
        return;
    }

    const bgfx::Memory* vsMem = bgfx::copy(vsBytes.data(), static_cast<uint32_t>(vsBytes.size()));
    const bgfx::Memory* fsMem = bgfx::copy(fsBytes.data(), static_cast<uint32_t>(fsBytes.size()));
    bgfx::ShaderHandle vsh = bgfx::createShader(vsMem);
    bgfx::ShaderHandle fsh = bgfx::createShader(fsMem);
    imguiProgram = bgfx::createProgram(vsh, fsh, true);
    if (!bgfx::isValid(imguiProgram)) {
        spdlog::error("UiSystem: failed to create ImGui bgfx shader program");
        return;
    }

    imguiLayout.begin()
        .add(bgfx::Attrib::Position, 2, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
        .end();

    imguiBgfxReady = true;
    spdlog::info("UiSystem: ImGui bgfx renderer init done");
}

void ImGuiBackend::shutdownBgfxRenderer() {
    if (!bgfx::getCaps()) {
        imguiBgfxReady = false;
        return;
    }
    if (bgfx::isValid(imguiFontTexture)) {
        bgfx::destroy(imguiFontTexture);
        imguiFontTexture = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(imguiProgram)) {
        bgfx::destroy(imguiProgram);
        imguiProgram = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(imguiTexture)) {
        bgfx::destroy(imguiTexture);
        imguiTexture = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(imguiScaleBias)) {
        bgfx::destroy(imguiScaleBias);
        imguiScaleBias = BGFX_INVALID_HANDLE;
    }
    imguiBgfxReady = false;
}

void ImGuiBackend::buildBgfxFonts() {
    if (!imguiBgfxReady) {
        spdlog::warn("UiSystem: ImGui bgfx renderer not ready; skipping font texture build");
        return;
    }
    spdlog::info("UiSystem: ImGui bgfx font build enter");
    ImGuiIO &io = ImGui::GetIO();
    unsigned char* pixels = nullptr;
    int width = 0;
    int height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    if (!pixels || width == 0 || height == 0) {
        spdlog::error("UiSystem: ImGui font texture build failed");
        return;
    }

    if (bgfx::isValid(imguiFontTexture)) {
        bgfx::destroy(imguiFontTexture);
    }

    const bgfx::Memory* mem = bgfx::copy(pixels, static_cast<uint32_t>(width * height * 4));
    imguiFontTexture = bgfx::createTexture2D(
        static_cast<uint16_t>(width),
        static_cast<uint16_t>(height),
        false,
        1,
        bgfx::TextureFormat::RGBA8,
        0,
        mem);

    if (!bgfx::isValid(imguiFontTexture)) {
        spdlog::error("UiSystem: failed to create ImGui font texture");
        return;
    }

    io.Fonts->SetTexID(static_cast<ImTextureID>(static_cast<uint64_t>(imguiFontTexture.idx + 1)));
    imguiFontsReady = true;
    spdlog::info("UiSystem: ImGui bgfx font build done");
}

void ImGuiBackend::renderBgfxDrawData(ImDrawData* drawData) {
    if (!drawData || !imguiBgfxReady || !bgfx::isValid(imguiProgram) || !bgfx::isValid(imguiFontTexture)) {
        return;
    }

    const ImGuiIO &io = ImGui::GetIO();
    const int fbWidth = static_cast<int>(io.DisplaySize.x * io.DisplayFramebufferScale.x);
    const int fbHeight = static_cast<int>(io.DisplaySize.y * io.DisplayFramebufferScale.y);
    if (fbWidth <= 0 || fbHeight <= 0) {
        return;
    }

    drawData->ScaleClipRects(io.DisplayFramebufferScale);

    const float scaleBias[4] = { 2.0f / io.DisplaySize.x, -2.0f / io.DisplaySize.y, -1.0f, 1.0f };
    bgfx::setViewTransform(255, nullptr, nullptr);
    bgfx::setViewRect(255, 0, 0, static_cast<uint16_t>(fbWidth), static_cast<uint16_t>(fbHeight));
    bgfx::setUniform(imguiScaleBias, scaleBias);

    for (int n = 0; n < drawData->CmdListsCount; n++) {
        const ImDrawList* cmdList = drawData->CmdLists[n];
        const ImDrawVert* vtxBuffer = cmdList->VtxBuffer.Data;
        const ImDrawIdx* idxBuffer = cmdList->IdxBuffer.Data;

        bgfx::TransientVertexBuffer tvb;
        bgfx::TransientIndexBuffer tib;
        const uint32_t availVb = bgfx::getAvailTransientVertexBuffer(cmdList->VtxBuffer.Size, imguiLayout);
        const uint32_t availIb = bgfx::getAvailTransientIndexBuffer(cmdList->IdxBuffer.Size, sizeof(ImDrawIdx) == 4);
        if (availVb < static_cast<uint32_t>(cmdList->VtxBuffer.Size)
            || availIb < static_cast<uint32_t>(cmdList->IdxBuffer.Size)) {
            continue;
        }
        bgfx::allocTransientVertexBuffer(&tvb, cmdList->VtxBuffer.Size, imguiLayout);
        bgfx::allocTransientIndexBuffer(&tib, cmdList->IdxBuffer.Size, sizeof(ImDrawIdx) == 4);

        auto* verts = reinterpret_cast<ImGuiVertex*>(tvb.data);
        for (int i = 0; i < cmdList->VtxBuffer.Size; ++i) {
            verts[i].x = vtxBuffer[i].pos.x;
            verts[i].y = vtxBuffer[i].pos.y;
            verts[i].u = vtxBuffer[i].uv.x;
            verts[i].v = vtxBuffer[i].uv.y;
            verts[i].abgr = vtxBuffer[i].col;
        }
        std::memcpy(tib.data, idxBuffer, static_cast<size_t>(cmdList->IdxBuffer.Size) * sizeof(ImDrawIdx));

        uint32_t vtxOffset = 0;
        uint32_t idxOffset = 0;
        for (int cmdIdx = 0; cmdIdx < cmdList->CmdBuffer.Size; cmdIdx++) {
            const ImDrawCmd* pcmd = &cmdList->CmdBuffer[cmdIdx];
            if (pcmd->UserCallback) {
                pcmd->UserCallback(cmdList, pcmd);
            } else {
                const ImVec4 clip = pcmd->ClipRect;
                const uint16_t cx = static_cast<uint16_t>(std::max(0.0f, clip.x));
                const uint16_t cy = static_cast<uint16_t>(std::max(0.0f, clip.y));
                const uint16_t cw = static_cast<uint16_t>(std::min(clip.z, io.DisplaySize.x) - clip.x);
                const uint16_t ch = static_cast<uint16_t>(std::min(clip.w, io.DisplaySize.y) - clip.y);
                if (cw == 0 || ch == 0) {
                    idxOffset += pcmd->ElemCount;
                    continue;
                }

                bgfx::setScissor(cx, cy, cw, ch);
                bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_MSAA |
                               BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_INV_SRC_ALPHA));

                bgfx::TextureHandle textureHandle = imguiFontTexture;
                if (pcmd->TextureId) {
                    const uint16_t idx = ToTextureHandle(pcmd->TextureId);
                    textureHandle.idx = idx;
                }
                bgfx::setTexture(0, imguiTexture, textureHandle);

                bgfx::setVertexBuffer(0, &tvb, vtxOffset, cmdList->VtxBuffer.Size);
                bgfx::setIndexBuffer(&tib, idxOffset, pcmd->ElemCount);
                bgfx::submit(255, imguiProgram);
            }
            idxOffset += pcmd->ElemCount;
        }
    }
}
#endif

} // namespace ui_backend
