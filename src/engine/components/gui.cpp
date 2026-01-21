#include "engine/components/gui.hpp"

#include "engine/components/gui/ui_backend.hpp"

#if defined(BZ3_UI_BACKEND_IMGUI)
#include "engine/components/gui/backends/imgui_backend.hpp"
#elif defined(BZ3_UI_BACKEND_RMLUI)
#include "engine/components/gui/backends/rmlui_backend.hpp"
#else
#error "BZ3 UI backend not set. Define BZ3_UI_BACKEND_IMGUI or BZ3_UI_BACKEND_RMLUI."
#endif

GUI::GUI(GLFWwindow *window) {
#if defined(BZ3_UI_BACKEND_IMGUI)
    backend = std::make_unique<gui::ImGuiBackend>(window);
#elif defined(BZ3_UI_BACKEND_RMLUI)
    backend = std::make_unique<gui::RmlUiBackend>(window);
#endif
}

GUI::~GUI() = default;

void GUI::update() {
    backend->update();
}

void GUI::reloadFonts() {
    backend->reloadFonts();
}

gui::MainMenuInterface &GUI::mainMenu() {
    return backend->mainMenu();
}

const gui::MainMenuInterface &GUI::mainMenu() const {
    return backend->mainMenu();
}

void GUI::setScoreboardEntries(const std::vector<ScoreboardEntry> &entries) {
    backend->setScoreboardEntries(entries);
}

void GUI::setSpawnHint(const std::string &hint) {
    backend->setSpawnHint(hint);
}

void GUI::setRadarTextureId(unsigned int textureId) {
    backend->setRadarTextureId(textureId);
}

void GUI::addConsoleLine(const std::string &playerName, const std::string &line) {
    backend->addConsoleLine(playerName, line);
}

std::string GUI::getChatInputBuffer() const {
    return backend->getChatInputBuffer();
}

void GUI::clearChatInputBuffer() {
    backend->clearChatInputBuffer();
}

void GUI::focusChatInput() {
    backend->focusChatInput();
}

bool GUI::getChatInputFocus() const {
    return backend->getChatInputFocus();
}

void GUI::displayDeathScreen(bool show) {
    backend->displayDeathScreen(show);
}
