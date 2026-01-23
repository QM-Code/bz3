#pragma once

#include <memory>
#include <string>
#include <vector>

#include "engine/components/gui/ui_backend.hpp"

struct GLFWwindow;
namespace Rml {
class Event;
}

namespace gui {

class RmlUiHud;

class RmlUiBackend final : public UiBackend {
public:
    explicit RmlUiBackend(GLFWwindow *window);
    ~RmlUiBackend() override;

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
    void setActiveTab(const std::string &tabKey);
    bool isUiInputEnabled() const;

private:
    GLFWwindow *window = nullptr;
    struct RmlUiState;
    std::unique_ptr<RmlUiState> state;
    std::unique_ptr<class RmlUiMainMenu> menu;
    void loadMenuDocument();
    void loadHudDocument();
    const std::string &cachedTwemojiMarkup(const std::string &text);
};

} // namespace gui
