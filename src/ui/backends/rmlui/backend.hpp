#pragma once

#include <memory>
#include <string>
#include <vector>

#include "ui/backend.hpp"

namespace platform {
class Window;
}

namespace Rml {
class Event;
}

namespace ui {
class RmlUiHud;
class RmlUiConsole;
}

namespace ui_backend {

class RmlUiBackend final : public Backend {
public:
    explicit RmlUiBackend(platform::Window &window);
    ~RmlUiBackend() override;

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
    void setActiveTab(const std::string &tabKey);
    bool isUiInputEnabled() const;

private:
    platform::Window *windowRef = nullptr;
    struct RmlUiState;
    std::unique_ptr<RmlUiState> state;
    std::unique_ptr<ui::RmlUiConsole> consoleView;
    const ui::RenderBridge *renderBridge = nullptr;
    void loadConsoleDocument();
    void loadHudDocument();
    const std::string &cachedTwemojiMarkup(const std::string &text);
};

} // namespace ui_backend
