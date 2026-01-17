#pragma once
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_glfw.h>      // your platform backend
#include <imgui_impl_opengl3.h>   // your renderer backend
#include <vector>
#include <string>
#include <array>
#include <cstdint>

#include "engine/components/gui/server_browser.hpp"

struct ScoreboardEntry {
    std::string name;
    int score;
    bool registeredUser = false;
    bool communityAdmin = false;
    bool localAdmin = false;
};

class GUI {
    friend class ClientEngine;

public:
    gui::ServerBrowserView &serverBrowser();
    const gui::ServerBrowserView &serverBrowser() const;

private:
    GLFWwindow *window;
    ImFont* bigFont;
    gui::ServerBrowserView serverBrowserView;
    bool showFPS = false;
    std::string spawnHint = "Press U to spawn";

    void update();
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

    GUI(GLFWwindow *window);
    ~GUI();

public:
    void setScoreboardEntries(const std::vector<ScoreboardEntry> &entries);
    void setSpawnHint(const std::string &hint);
    void setRadarTextureId(unsigned int textureId) { radarTextureId = textureId; }
    void addConsoleLine(const std::string &playerName, const std::string &line);
    std::string getChatInputBuffer() const;
    void clearChatInputBuffer();
    void focusChatInput();
    bool getChatInputFocus() const;
    void displayDeathScreen(bool show);
};
