#include "ui/backends/rmlui/console/console.hpp"

#include "ui/backends/rmlui/console/panels/panel_community.hpp"
#include "ui/backends/rmlui/console/panels/panel_start_server.hpp"
#include "ui/backends/rmlui/console/panels/panel_settings.hpp"

#include <filesystem>

#include "common/data_path_resolver.hpp"
#include "spdlog/spdlog.h"

namespace ui {

RmlUiConsole::RmlUiConsole() = default;

void RmlUiConsole::attachCommunityPanel(RmlUiPanelCommunity *panel) {
    communityPanel = panel;
    applyListOptionsToPanel();
    if (communityPanel) {
        communityPanel->setConnectionState(connectionState);
        if (!userConfigPath.empty()) {
            communityPanel->setUserConfigPath(userConfigPath);
        }
    }
}

void RmlUiConsole::attachStartServerPanel(RmlUiPanelStartServer *panel) {
    startServerPanel = panel;
    if (startServerPanel) {
        startServerPanel->setListOptions(listOptions, listSelectedIndex);
    }
}

void RmlUiConsole::attachSettingsPanel(RmlUiPanelSettings *panel) {
    settingsPanel = panel;
    if (settingsPanel && !userConfigPath.empty()) {
        settingsPanel->setUserConfigPath(userConfigPath);
    }
}

void RmlUiConsole::show(const std::vector<CommunityBrowserEntry> &entriesIn) {
    if (!entriesIn.empty()) {
        entries = entriesIn;
    }
    visible = true;
    if (communityPanel) {
        communityPanel->setEntries(entries);
    }
}

void RmlUiConsole::setEntries(const std::vector<CommunityBrowserEntry> &entriesIn) {
    entries = entriesIn;
    if (selectedServerIndex >= static_cast<int>(entries.size())) {
        selectedServerIndex = -1;
    }
    if (communityPanel) {
        communityPanel->setEntries(entries);
    }
}

void RmlUiConsole::setListOptions(const std::vector<ServerListOption> &options, int selectedIndex) {
    listOptions = options;
    if (listOptions.empty()) {
        listSelectedIndex = -1;
        return;
    }
    if (selectedIndex < 0) {
        listSelectedIndex = 0;
    } else if (selectedIndex >= static_cast<int>(listOptions.size())) {
        listSelectedIndex = static_cast<int>(listOptions.size()) - 1;
    } else {
        listSelectedIndex = selectedIndex;
    }
    applyListOptionsToPanel();
    refreshCommunityCredentials();
    if (startServerPanel) {
        startServerPanel->setListOptions(listOptions, listSelectedIndex);
    }
}

void RmlUiConsole::hide() {
    visible = false;
}

bool RmlUiConsole::isVisible() const {
    return visible;
}

void RmlUiConsole::setStatus(const std::string &, bool) {}

void RmlUiConsole::setCommunityDetails(const std::string &detailsText) {
    if (communityPanel) {
        communityPanel->setCommunityDetails(detailsText);
    }
}

void RmlUiConsole::setServerDescriptionLoading(const std::string &key, bool loading) {
    serverDescriptionLoadingKey = key;
    serverDescriptionLoading = loading;
    if (!loading && key.empty()) {
        serverDescriptionLoadingKey.clear();
    }
    if (communityPanel) {
        communityPanel->setServerDescriptionLoading(key, loading);
    }
}

bool RmlUiConsole::isServerDescriptionLoading(const std::string &key) const {
    return serverDescriptionLoading && key == serverDescriptionLoadingKey;
}

void RmlUiConsole::setServerDescriptionError(const std::string &key, const std::string &message) {
    serverDescriptionErrorKey = key;
    serverDescriptionErrorText = message;
    if (communityPanel) {
        communityPanel->setServerDescriptionError(key, message);
    }
}

std::optional<std::string> RmlUiConsole::getServerDescriptionError(const std::string &key) const {
    if (key.empty() || key != serverDescriptionErrorKey) {
        return std::nullopt;
    }
    return serverDescriptionErrorText;
}

std::optional<CommunityBrowserSelection> RmlUiConsole::consumeSelection() {
    if (!pendingSelection.has_value()) {
        return std::nullopt;
    }
    auto selection = pendingSelection;
    pendingSelection.reset();
    return selection;
}

std::optional<int> RmlUiConsole::consumeListSelection() {
    if (!pendingListSelection.has_value()) {
        return std::nullopt;
    }
    auto out = pendingListSelection;
    pendingListSelection.reset();
    return out;
}

std::optional<ServerListOption> RmlUiConsole::consumeNewListRequest() {
    if (!pendingNewList.has_value()) {
        return std::nullopt;
    }
    auto out = pendingNewList;
    pendingNewList.reset();
    return out;
}

std::optional<std::string> RmlUiConsole::consumeDeleteListRequest() {
    if (communityPanel) {
        return communityPanel->consumeDeleteListRequest();
    }
    return std::nullopt;
}


void RmlUiConsole::setListStatus(const std::string &statusText, bool isErrorMessage) {
    listStatusText = statusText;
    listStatusIsError = isErrorMessage;
    if (communityPanel) {
        communityPanel->setAddStatus(listStatusText, listStatusIsError);
    }
}

void RmlUiConsole::clearNewListInputs() {
    if (communityPanel) {
        communityPanel->clearAddInput();
    }
}

std::string RmlUiConsole::getUsername() const {
    if (communityPanel) {
        return communityPanel->getUsernameValue();
    }
    return {};
}

std::string RmlUiConsole::getPassword() const {
    if (communityPanel) {
        return communityPanel->getPasswordValue();
    }
    return {};
}

std::string RmlUiConsole::getStoredPasswordHash() const {
    if (communityPanel) {
        return communityPanel->getStoredPasswordHashValue();
    }
    return {};
}

void RmlUiConsole::clearPassword() {
    if (communityPanel) {
        communityPanel->clearPasswordValue();
    }
}

void RmlUiConsole::storeCommunityAuth(const std::string &communityHost,
                                       const std::string &username,
                                       const std::string &passhash,
                                       const std::string &salt) {
    (void)communityHost;
    (void)salt;
    if (communityPanel && !username.empty()) {
        communityPanel->setUsernameValue(username);
    }
    if (communityPanel && !passhash.empty()) {
        communityPanel->setStoredPasswordHashValue(passhash);
        communityPanel->persistCommunityCredentials(false);
    }
}

void RmlUiConsole::setCommunityStatus(const std::string &, MessageTone) {}

std::optional<CommunityBrowserEntry> RmlUiConsole::getSelectedEntry() const {
    if (selectedServerIndex < 0 || selectedServerIndex >= static_cast<int>(entries.size())) {
        return std::nullopt;
    }
    return entries[static_cast<std::size_t>(selectedServerIndex)];
}

bool RmlUiConsole::consumeRefreshRequest() {
    if (!pendingRefresh) {
        return false;
    }
    pendingRefresh = false;
    return true;
}

void RmlUiConsole::setScanning(bool) {}

void RmlUiConsole::setUserConfigPath(const std::string &path) {
    userConfigPath = path;
    refreshCommunityCredentials();
    if (communityPanel) {
        communityPanel->setUserConfigPath(path);
    }
    if (settingsPanel) {
        settingsPanel->setUserConfigPath(path);
    }
}

bool RmlUiConsole::consumeFontReloadRequest() {
    return false;
}

bool RmlUiConsole::consumeKeybindingsReloadRequest() {
    if (!settingsPanel) {
        return false;
    }
    return settingsPanel->consumeKeybindingsReloadRequest();
}

void RmlUiConsole::setConnectionState(const ConnectionState &state) {
    connectionState = state;
    if (communityPanel) {
        communityPanel->setConnectionState(state);
    }
}

ConsoleInterface::ConnectionState RmlUiConsole::getConnectionState() const {
    return connectionState;
}

bool RmlUiConsole::consumeQuitRequest() {
    if (!pendingQuitRequest) {
        return false;
    }
    pendingQuitRequest = false;
    return true;
}

void RmlUiConsole::showErrorDialog(const std::string &message) {
    if (communityPanel) {
        communityPanel->showErrorDialog(message);
    }
}

void RmlUiConsole::onCommunitySelection(int index) {
    if (index < 0 || index >= static_cast<int>(listOptions.size())) {
        return;
    }
    if (listSelectedIndex != index) {
        listSelectedIndex = index;
        pendingListSelection = index;
        selectedServerIndex = -1;
    }
    refreshCommunityCredentials();
}

void RmlUiConsole::onCommunityAddRequested(const std::string &host) {
    if (host.empty()) {
        return;
    }
    pendingNewList = ServerListOption{std::string{}, host};
}

void RmlUiConsole::onCommunityAddCanceled() {
    if (communityPanel) {
        communityPanel->clearAddInput();
    }
}

void RmlUiConsole::onRefreshRequested() {
    pendingRefresh = true;
}

void RmlUiConsole::onServerSelection(int index) {
    if (index < 0 || index >= static_cast<int>(entries.size())) {
        return;
    }
    selectedServerIndex = index;
}

bool RmlUiConsole::loadUserConfig(bz::json::Value &out) const {
    const std::filesystem::path path = userConfigPath.empty()
        ? bz::data::EnsureUserConfigFile("config.json")
        : std::filesystem::path(userConfigPath);
    if (auto user = bz::data::LoadJsonFile(path, "user config", spdlog::level::debug)) {
        if (!user->is_object()) {
            out = bz::json::Object();
            return false;
        }
        out = *user;
        return true;
    }
    out = bz::json::Object();
    return true;
}


std::string RmlUiConsole::communityKeyForIndex(int index) const {
    if (index < 0 || index >= static_cast<int>(listOptions.size())) {
        return {};
    }
    const auto &option = listOptions[index];
    if (option.name == "Local Area Network") {
        return "LAN";
    }
    std::string host = option.host;
    while (!host.empty() && host.back() == '/') {
        host.pop_back();
    }
    return host;
}

void RmlUiConsole::refreshCommunityCredentials() {
    if (listSelectedIndex == lastCredentialsListIndex) {
        return;
    }
    lastCredentialsListIndex = listSelectedIndex;
    if (communityPanel) {
        communityPanel->refreshCommunityCredentials();
    }
}


void RmlUiConsole::onJoinRequested(int index) {
    if (index < 0 || index >= static_cast<int>(entries.size())) {
        spdlog::warn("RmlUi Console: Join requested with invalid index {}", index);
        return;
    }
    const auto &entry = entries[static_cast<std::size_t>(index)];
    pendingSelection = CommunityBrowserSelection{
        entry.host,
        entry.port,
        true,
        entry.sourceHost,
        entry.worldName
    };
    spdlog::info("RmlUi Console: Join queued host={} port={} sourceHost={} worldName={}",
                 entry.host, entry.port, entry.sourceHost, entry.worldName);
}

void RmlUiConsole::onQuitRequested() {
    pendingQuitRequest = true;
}

void RmlUiConsole::applyListOptionsToPanel() {
    if (!communityPanel) {
        return;
    }
    communityPanel->setListOptions(listOptions, listSelectedIndex);
}

} // namespace ui
