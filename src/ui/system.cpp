#include "ui/system.hpp"

#include "ui/backend.hpp"

#if defined(BZ3_UI_BACKEND_IMGUI)
#include "ui/backends/imgui/backend.hpp"
#elif defined(BZ3_UI_BACKEND_RMLUI)
#include "ui/backends/rmlui/backend.hpp"
#else
#error "BZ3 UI backend not set. Define BZ3_UI_BACKEND_IMGUI or BZ3_UI_BACKEND_RMLUI."
#endif

UiSystem::UiSystem(GLFWwindow *window) {
#if defined(BZ3_UI_BACKEND_IMGUI)
    backend = std::make_unique<ui::ImGuiBackend>(window);
#elif defined(BZ3_UI_BACKEND_RMLUI)
    backend = std::make_unique<ui::RmlUiBackend>(window);
#endif
}

UiSystem::~UiSystem() = default;

void UiSystem::update() {
    backend->update();
}

void UiSystem::reloadFonts() {
    backend->reloadFonts();
}

ui::ConsoleInterface &UiSystem::console() {
    return backend->console();
}

const ui::ConsoleInterface &UiSystem::console() const {
    return backend->console();
}

void UiSystem::setScoreboardEntries(const std::vector<ScoreboardEntry> &entries) {
    backend->setScoreboardEntries(entries);
}

void UiSystem::setSpawnHint(const std::string &hint) {
    backend->setSpawnHint(hint);
}

void UiSystem::setRadarTextureId(unsigned int textureId) {
    backend->setRadarTextureId(textureId);
}

void UiSystem::addConsoleLine(const std::string &playerName, const std::string &line) {
    backend->addConsoleLine(playerName, line);
}

std::string UiSystem::getChatInputBuffer() const {
    return backend->getChatInputBuffer();
}

void UiSystem::clearChatInputBuffer() {
    backend->clearChatInputBuffer();
}

void UiSystem::focusChatInput() {
    backend->focusChatInput();
}

bool UiSystem::getChatInputFocus() const {
    return backend->getChatInputFocus();
}

void UiSystem::displayDeathScreen(bool show) {
    backend->displayDeathScreen(show);
}
