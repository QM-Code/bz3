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
    void drawTexture(unsigned int textureId);
};

} // namespace ui_backend
