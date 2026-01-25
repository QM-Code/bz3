#include "ui/backends/imgui/backend.hpp"

#include <imgui_impl_opengl3.h>

#include "common/data_path_resolver.hpp"
#include "common/config_helpers.hpp"
#include "platform/window.hpp"
#include "spdlog/spdlog.h"

#include <algorithm>

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

    ImGui_ImplOpenGL3_Init("#version 330");

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

    showFPS = bz::data::ReadBoolConfig({"debug.ShowFPS"}, false);
    hud.setShowFps(showFPS);

    io.Fonts->Build();
}

ImGuiBackend::~ImGuiBackend() {
    ImGui_ImplOpenGL3_Shutdown();
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
    if (consoleView.consumeFontReloadRequest()) {
        reloadFonts();
    }
    if (renderBridge) {
        hud.setRadarTextureId(renderBridge->getRadarTextureId());
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

    ImGui_ImplOpenGL3_NewFrame();
    io.FontGlobalScale = 1.0f;
    ImGui::NewFrame();

    if (consoleView.isVisible()) {
        consoleView.draw(io);
    } else {
        hud.setShowFps(showFPS);
        hud.draw(io, bigFont);
    }

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
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
    ImGui_ImplOpenGL3_DestroyFontsTexture();
    ImGui_ImplOpenGL3_CreateFontsTexture();
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

} // namespace ui_backend
