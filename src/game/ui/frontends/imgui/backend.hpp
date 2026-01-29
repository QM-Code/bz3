#pragma once

#include <array>
#include <chrono>
#include <optional>
#include <string>
#include <vector>

#include <imgui.h>

#include "ui/frontends/imgui/hud/hud.hpp"
#include "ui/frontends/imgui/console/console.hpp"
#include "ui/bridges/imgui_render_bridge.hpp"
#include "ui/backend.hpp"

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

    void setHudModel(const ui::HudModel &model) override;
    void addConsoleLine(const std::string &playerName, const std::string &line) override;
    std::string getChatInputBuffer() const override;
    void clearChatInputBuffer() override;
    void focusChatInput() override;
    bool getChatInputFocus() const override;
    bool consumeKeybindingsReloadRequest() override;
    void setRenderBridge(const ui::RenderBridge *bridge) override;
    void setImGuiRenderBridge(const ui::ImGuiRenderBridge *bridge) override;
    ui::RenderOutput getRenderOutput() const override;
    float getRenderBrightness() const override { return consoleView.getRenderBrightness(); }
    bool isRenderBrightnessDragActive() const override;

private:
    platform::Window *window = nullptr;
    std::chrono::steady_clock::time_point lastFrameTime;
    ImFont *bigFont = nullptr;
    ui::ConsoleView consoleView;
    ui::ImGuiHud hud;
    ui::HudModel hudModel;
    const ui::RenderBridge *renderBridge = nullptr;
    const ui::ImGuiRenderBridge* imguiBridge = nullptr;
    graphics_backend::UiRenderTargetBridge* uiBridge = nullptr;
    bool languageReloadArmed = false;
    std::optional<std::string> pendingLanguage;
    bool fontsDirty = false;
    bool outputVisible = false;
    void drawTexture(const graphics::TextureHandle& texture);
};

} // namespace ui_backend
