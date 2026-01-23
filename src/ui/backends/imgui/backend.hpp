#pragma once

#include <array>
#include <string>
#include <vector>

#include <GLFW/glfw3.h>
#include <imgui.h>

#include "ui/backends/imgui/hud/hud.hpp"
#include "ui/backends/imgui/console/console.hpp"
#include "ui/backend.hpp"

namespace ui {

class ImGuiBackend final : public UiBackend {
public:
    explicit ImGuiBackend(GLFWwindow *window);
    ~ImGuiBackend() override;

    ConsoleInterface &console() override;
    const ConsoleInterface &console() const override;
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
    ConsoleView consoleView;
    ImGuiHud hud;
    bool showFPS = false;
    void drawTexture(unsigned int textureId);
};

} // namespace ui
