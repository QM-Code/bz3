#include "ui/core/system.hpp"

#include "ui/core/backend.hpp"
#include "platform/window.hpp"
#include "common/i18n.hpp"
#include "ui/config/ui_config.hpp"
#include "common/config_store.hpp"
#include "spdlog/spdlog.h"

UiSystem::UiSystem(platform::Window &window)
    : hudController(hudModel) {
    backend = ui_backend::CreateUiBackend(window);
}

UiSystem::~UiSystem() = default;

void UiSystem::handleEvents(const std::vector<platform::Event> &events) {
    backend->handleEvents(events);
}

void UiSystem::update() {
    const uint64_t revision = bz::config::ConfigStore::Revision();
    if (revision != lastConfigRevision) {
        lastConfigRevision = revision;
        hudModel.visibility.scoreboard = ui::UiConfig::GetHudScoreboard();
        hudModel.visibility.chat = ui::UiConfig::GetHudChat();
        hudModel.visibility.radar = ui::UiConfig::GetHudRadar();
        hudModel.visibility.fps = ui::UiConfig::GetHudFps();
        hudModel.visibility.crosshair = ui::UiConfig::GetHudCrosshair();
    }
    const bool consoleVisible = backend->console().isVisible();
    const bool connected = backend->console().getConnectionState().connected;
    hudModel.visibility.hud = connected || !consoleVisible;
    hudController.tick();
    backend->setHudModel(hudModel);
    backend->update();
}

void UiSystem::reloadFonts() {
    backend->reloadFonts();
}

void UiSystem::setLanguage(const std::string &language) {
    bz::i18n::Get().loadLanguage(language);
    backend->reloadFonts();
}

ui::ConsoleInterface &UiSystem::console() {
    return backend->console();
}

const ui::ConsoleInterface &UiSystem::console() const {
    return backend->console();
}

void UiSystem::setScoreboardEntries(const std::vector<ScoreboardEntry> &entries) {
    hudModel.scoreboardEntries = entries;
}

void UiSystem::setDialogText(const std::string &text) {
    hudController.setDialogText(text);
}

void UiSystem::addConsoleLine(const std::string &playerName, const std::string &line) {
    hudController.addChatLine(playerName, line);
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

void UiSystem::setDialogVisible(bool show) {
    hudController.setDialogVisible(show);
}

bool UiSystem::consumeKeybindingsReloadRequest() {
    return backend->consumeKeybindingsReloadRequest();
}

void UiSystem::setRenderBridge(const ui::RenderBridge *bridge) {
    backend->setRenderBridge(bridge);
}

ui::RenderOutput UiSystem::getRenderOutput() const {
    return backend ? backend->getRenderOutput() : ui::RenderOutput{};
}

float UiSystem::getRenderBrightness() const {
    if (!backend) {
        return 1.0f;
    }
    if (backend->isRenderBrightnessDragActive()) {
        return 1.0f;
    }
    return backend->getRenderBrightness();
}
