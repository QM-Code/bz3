#pragma once

#include <array>
#include <chrono>
#include <optional>
#include <string>
#include <vector>

#include <imgui.h>

#include "ui/backends/imgui/hud/hud.hpp"
#include "ui/backends/imgui/console/console.hpp"
#include "ui/backend.hpp"

#if defined(BZ3_RENDER_BACKEND_BGFX)
#include <bgfx/bgfx.h>
#endif

namespace platform {
class Window;
}

namespace ui_backend {

class ImGuiBackend final : public Backend {
public:
    explicit ImGuiBackend(platform::Window &window);
    ~ImGuiBackend() override;

    ui::ConsoleInterface &console() override;
    const ui::ConsoleInterface &console() const override;
    void handleEvents(const std::vector<platform::Event> &events) override;
    void update() override;
    void reloadFonts() override;

    void setScoreboardEntries(const std::vector<ScoreboardEntry> &entries) override;
    void setSpawnHint(const std::string &hint) override;
    void addConsoleLine(const std::string &playerName, const std::string &line) override;
    std::string getChatInputBuffer() const override;
    void clearChatInputBuffer() override;
    void focusChatInput() override;
    bool getChatInputFocus() const override;
    void displayDeathScreen(bool show) override;
    bool consumeKeybindingsReloadRequest() override;
    void setRenderBridge(const ui::RenderBridge *bridge) override;
    ui::RenderOutput getRenderOutput() const override;
    float getRenderBrightness() const override { return consoleView.getRenderBrightness(); }

private:
    platform::Window *window = nullptr;
    std::chrono::steady_clock::time_point lastFrameTime;
    ImFont *bigFont = nullptr;
    ui::ConsoleView consoleView;
    ui::ImGuiHud hud;
    bool showFPS = false;
    const ui::RenderBridge *renderBridge = nullptr;
    bool languageReloadArmed = false;
    std::optional<std::string> pendingLanguage;
    void drawTexture(const graphics::TextureHandle& texture);

#if defined(BZ3_RENDER_BACKEND_BGFX)
    bgfx::ProgramHandle imguiProgram = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle imguiTexture = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle imguiScaleBias = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle imguiFontTexture = BGFX_INVALID_HANDLE;
    bgfx::VertexLayout imguiLayout;
    bool imguiBgfxReady = false;
    bool imguiFontsReady = false;

    void initBgfxRenderer();
    void shutdownBgfxRenderer();
    void buildBgfxFonts();
    void renderBgfxDrawData(ImDrawData* drawData);
#endif
};

} // namespace ui_backend
