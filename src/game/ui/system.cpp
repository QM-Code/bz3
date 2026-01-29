#include "ui/system.hpp"

#include "ui/backend.hpp"
#include "platform/window.hpp"
#include "common/i18n.hpp"
#include "ui/bridges/imgui_render_bridge.hpp"
#include "ui/ui_config.hpp"
#include "common/config_store.hpp"
#include "spdlog/spdlog.h"

UiSystem::UiSystem(platform::Window &window) {
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
    hudModel.dialog.text = text;
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

void UiSystem::setDialogVisible(bool show) {
    hudModel.dialog.visible = show;
}

bool UiSystem::consumeKeybindingsReloadRequest() {
    return backend->consumeKeybindingsReloadRequest();
}

void UiSystem::setRenderBridge(const ui::RenderBridge *bridge) {
    backend->setRenderBridge(bridge);
}

void UiSystem::setImGuiRenderBridge(const ui::ImGuiRenderBridge *bridge) {
    backend->setImGuiRenderBridge(bridge);
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
