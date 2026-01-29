#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "platform/events.hpp"
#include "ui/hud_model.hpp"
#include "ui/types.hpp"
#include "ui/console/console_interface.hpp"
#include "ui/bridges/render_bridge.hpp"

namespace platform {
class Window;
}

namespace ui_backend {
class Backend;
}

namespace ui {
class ImGuiRenderBridge;
}

class UiSystem {
    friend class ClientEngine;

public:
    ui::ConsoleInterface &console();
    const ui::ConsoleInterface &console() const;

private:
    std::unique_ptr<ui_backend::Backend> backend;
    ui::HudModel hudModel;
    uint64_t lastConfigRevision = 0;

    void update();
    void reloadFonts();

    UiSystem(platform::Window &window);
    ~UiSystem();

public:
    void handleEvents(const std::vector<platform::Event> &events);
    void setLanguage(const std::string &language);
    void setScoreboardEntries(const std::vector<ScoreboardEntry> &entries);
    void setDialogText(const std::string &text);
    void addConsoleLine(const std::string &playerName, const std::string &line);
    std::string getChatInputBuffer() const;
    void clearChatInputBuffer();
    void focusChatInput();
    bool getChatInputFocus() const;
    void setDialogVisible(bool show);
    bool consumeKeybindingsReloadRequest();
    void setRenderBridge(const ui::RenderBridge *bridge);
    void setImGuiRenderBridge(const ui::ImGuiRenderBridge *bridge);
    ui::RenderOutput getRenderOutput() const;
    float getRenderBrightness() const;
};
