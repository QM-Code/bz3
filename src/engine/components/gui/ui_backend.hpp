#pragma once

#include <string>
#include <vector>

#include "engine/components/gui/gui_types.hpp"
#include "engine/components/gui/main_menu_interface.hpp"

struct GLFWwindow;

namespace gui {

class UiBackend {
public:
    virtual ~UiBackend() = default;

    virtual MainMenuInterface &mainMenu() = 0;
    virtual const MainMenuInterface &mainMenu() const = 0;
    virtual void update() = 0;
    virtual void reloadFonts() = 0;

    virtual void setScoreboardEntries(const std::vector<ScoreboardEntry> &entries) = 0;
    virtual void setSpawnHint(const std::string &hint) = 0;
    virtual void setRadarTextureId(unsigned int textureId) = 0;
    virtual void addConsoleLine(const std::string &playerName, const std::string &line) = 0;
    virtual std::string getChatInputBuffer() const = 0;
    virtual void clearChatInputBuffer() = 0;
    virtual void focusChatInput() = 0;
    virtual bool getChatInputFocus() const = 0;
    virtual void displayDeathScreen(bool show) = 0;
};

} // namespace gui
