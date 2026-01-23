#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "engine/components/gui/gui_types.hpp"
#include "engine/components/gui/main_menu_interface.hpp"

struct GLFWwindow;
namespace gui {
class UiBackend;
}

class GUI {
    friend class ClientEngine;

public:
    gui::MainMenuInterface &mainMenu();
    const gui::MainMenuInterface &mainMenu() const;

private:
    std::unique_ptr<gui::UiBackend> backend;

    void update();
    void reloadFonts();

    GUI(GLFWwindow *window);
    ~GUI();

public:
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
