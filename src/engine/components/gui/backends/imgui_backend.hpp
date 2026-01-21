#pragma once

#include <array>
#include <string>
#include <vector>

#include <GLFW/glfw3.h>
#include <imgui.h>

#include "engine/components/gui/main_menu.hpp"
#include "engine/components/gui/ui_backend.hpp"

namespace gui {

class ImGuiBackend final : public UiBackend {
public:
    explicit ImGuiBackend(GLFWwindow *window);
    ~ImGuiBackend() override;

    MainMenuInterface &mainMenu() override;
    const MainMenuInterface &mainMenu() const override;
    void update() override;
    void reloadFonts() override;

    void setScoreboardEntries(const std::vector<ScoreboardEntry> &entries) override;
    void setSpawnHint(const std::string &hint) override;
    void setRadarTextureId(unsigned int textureId) override;
    void addConsoleLine(const std::string &playerName, const std::string &line) override;
    std::string getChatInputBuffer() const override;
    void clearChatInputBuffer() override;
    void focusChatInput() override;
    bool getChatInputFocus() const override;
    void displayDeathScreen(bool show) override;

private:
    GLFWwindow *window = nullptr;
    ImFont *bigFont = nullptr;
    MainMenuView mainMenuView;
    bool showFPS = false;
    std::string spawnHint = "Press U to spawn";

    std::vector<ScoreboardEntry> scoreboardEntries;
    std::vector<std::string> consoleLines;

    std::array<char, 256> chatInputBuffer{};
    std::string submittedInputBuffer;
    bool chatFocus = false;

    unsigned int radarTextureId = 0;

    void drawTexture(unsigned int textureId);
    void drawConsolePanel();
    void drawDeathScreen();
    bool drawDeathScreenFlag = false;
};

} // namespace gui
