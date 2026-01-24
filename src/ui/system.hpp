#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "platform/events.hpp"
#include "ui/types.hpp"
#include "ui/console/console_interface.hpp"

namespace platform {
class Window;
}

namespace ui_backend {
class Backend;
}

class UiSystem {
    friend class ClientEngine;

public:
    ui::ConsoleInterface &console();
    const ui::ConsoleInterface &console() const;

private:
    std::unique_ptr<ui_backend::Backend> backend;

    void update();
    void reloadFonts();

    UiSystem(platform::Window &window);
    ~UiSystem();

public:
    void handleEvents(const std::vector<platform::Event> &events);
    void setScoreboardEntries(const std::vector<ScoreboardEntry> &entries);
    void setSpawnHint(const std::string &hint);
    void setRadarTextureId(unsigned int textureId);
    void addConsoleLine(const std::string &playerName, const std::string &line);
    std::string getChatInputBuffer() const;
    void clearChatInputBuffer();
    void focusChatInput();
    bool getChatInputFocus() const;
    void displayDeathScreen(bool show);
};
