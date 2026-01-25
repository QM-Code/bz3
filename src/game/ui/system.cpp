#include "ui/system.hpp"

#include "ui/backend.hpp"
#include "platform/window.hpp"

UiSystem::UiSystem(platform::Window &window) {
    backend = ui_backend::CreateUiBackend(window);
}

UiSystem::~UiSystem() = default;

void UiSystem::handleEvents(const std::vector<platform::Event> &events) {
    backend->handleEvents(events);
}

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

bool UiSystem::consumeKeybindingsReloadRequest() {
    return backend->consumeKeybindingsReloadRequest();
}

void UiSystem::setRenderBridge(const ui::RenderBridge *bridge) {
    backend->setRenderBridge(bridge);
}
