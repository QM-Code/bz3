#include "engine/components/gui/rmlui_main_menu.hpp"

#include "engine/components/gui/rmlui_panels/rmlui_panel_community.hpp"
#include "engine/components/gui/rmlui_panels/rmlui_panel_start_server.hpp"
#include "engine/components/gui/rmlui_panels/rmlui_panel_settings.hpp"

#include <filesystem>

#include "common/data_path_resolver.hpp"
#include "spdlog/spdlog.h"

namespace gui {

RmlUiMainMenu::RmlUiMainMenu() = default;

void RmlUiMainMenu::attachCommunityPanel(RmlUiPanelCommunity *panel) {
    communityPanel = panel;
    applyListOptionsToPanel();
    if (communityPanel) {
        communityPanel->setConnectionState(connectionState);
        if (!userConfigPath.empty()) {
            communityPanel->setUserConfigPath(userConfigPath);
        }
    }
}

void RmlUiMainMenu::attachStartServerPanel(RmlUiPanelStartServer *panel) {
    startServerPanel = panel;
    if (startServerPanel) {
        startServerPanel->setListOptions(listOptions, listSelectedIndex);
    }
}

void RmlUiMainMenu::attachSettingsPanel(RmlUiPanelSettings *panel) {
    settingsPanel = panel;
    if (settingsPanel && !userConfigPath.empty()) {
        settingsPanel->setUserConfigPath(userConfigPath);
    }
}

void RmlUiMainMenu::show(const std::vector<CommunityBrowserEntry> &entriesIn) {
    if (!entriesIn.empty()) {
        entries = entriesIn;
    }
    visible = true;
    if (communityPanel) {
        communityPanel->setEntries(entries);
    }
}

void RmlUiMainMenu::setEntries(const std::vector<CommunityBrowserEntry> &entriesIn) {
    entries = entriesIn;
    if (selectedServerIndex >= static_cast<int>(entries.size())) {
        selectedServerIndex = -1;
    }
    if (communityPanel) {
        communityPanel->setEntries(entries);
    }
}

void RmlUiMainMenu::setListOptions(const std::vector<ServerListOption> &options, int selectedIndex) {
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

void RmlUiMainMenu::hide() {
    visible = false;
}

bool RmlUiMainMenu::isVisible() const {
    return visible;
}

void RmlUiMainMenu::setStatus(const std::string &, bool) {}

void RmlUiMainMenu::setCommunityDetails(const std::string &detailsText) {
    if (communityPanel) {
        communityPanel->setCommunityDetails(detailsText);
    }
}

void RmlUiMainMenu::setServerDescriptionLoading(const std::string &key, bool loading) {
    serverDescriptionLoadingKey = key;
    serverDescriptionLoading = loading;
    if (!loading && key.empty()) {
        serverDescriptionLoadingKey.clear();
    }
    if (communityPanel) {
        communityPanel->setServerDescriptionLoading(key, loading);
    }
}

bool RmlUiMainMenu::isServerDescriptionLoading(const std::string &key) const {
    return serverDescriptionLoading && key == serverDescriptionLoadingKey;
}

void RmlUiMainMenu::setServerDescriptionError(const std::string &key, const std::string &message) {
    serverDescriptionErrorKey = key;
    serverDescriptionErrorText = message;
    if (communityPanel) {
        communityPanel->setServerDescriptionError(key, message);
    }
}

std::optional<std::string> RmlUiMainMenu::getServerDescriptionError(const std::string &key) const {
    if (key.empty() || key != serverDescriptionErrorKey) {
        return std::nullopt;
    }
    return serverDescriptionErrorText;
}

std::optional<CommunityBrowserSelection> RmlUiMainMenu::consumeSelection() {
    if (!pendingSelection.has_value()) {
        return std::nullopt;
    }
    auto selection = pendingSelection;
    pendingSelection.reset();
    return selection;
}

std::optional<int> RmlUiMainMenu::consumeListSelection() {
    if (!pendingListSelection.has_value()) {
        return std::nullopt;
    }
    auto out = pendingListSelection;
    pendingListSelection.reset();
    return out;
}

std::optional<ServerListOption> RmlUiMainMenu::consumeNewListRequest() {
    if (!pendingNewList.has_value()) {
        return std::nullopt;
    }
    auto out = pendingNewList;
    pendingNewList.reset();
    return out;
}

std::optional<std::string> RmlUiMainMenu::consumeDeleteListRequest() {
    if (communityPanel) {
        return communityPanel->consumeDeleteListRequest();
    }
    return std::nullopt;
}


void RmlUiMainMenu::setListStatus(const std::string &statusText, bool isErrorMessage) {
    listStatusText = statusText;
    listStatusIsError = isErrorMessage;
    if (communityPanel) {
        communityPanel->setAddStatus(listStatusText, listStatusIsError);
    }
}

void RmlUiMainMenu::clearNewListInputs() {
    if (communityPanel) {
        communityPanel->clearAddInput();
    }
}

std::string RmlUiMainMenu::getUsername() const {
    if (communityPanel) {
        return communityPanel->getUsernameValue();
    }
    return {};
}

std::string RmlUiMainMenu::getPassword() const {
    if (communityPanel) {
        return communityPanel->getPasswordValue();
    }
    return {};
}

std::string RmlUiMainMenu::getStoredPasswordHash() const {
    if (communityPanel) {
        return communityPanel->getStoredPasswordHashValue();
    }
    return {};
}

void RmlUiMainMenu::clearPassword() {
    if (communityPanel) {
        communityPanel->clearPasswordValue();
    }
}

void RmlUiMainMenu::storeCommunityAuth(const std::string &communityHost,
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

void RmlUiMainMenu::setCommunityStatus(const std::string &, MessageTone) {}

std::optional<CommunityBrowserEntry> RmlUiMainMenu::getSelectedEntry() const {
    if (selectedServerIndex < 0 || selectedServerIndex >= static_cast<int>(entries.size())) {
        return std::nullopt;
    }
    return entries[static_cast<std::size_t>(selectedServerIndex)];
}

bool RmlUiMainMenu::consumeRefreshRequest() {
    if (!pendingRefresh) {
        return false;
    }
    pendingRefresh = false;
    return true;
}

void RmlUiMainMenu::setScanning(bool) {}

void RmlUiMainMenu::setUserConfigPath(const std::string &path) {
    userConfigPath = path;
    refreshCommunityCredentials();
    if (communityPanel) {
        communityPanel->setUserConfigPath(path);
    }
    if (settingsPanel) {
        settingsPanel->setUserConfigPath(path);
    }
}

bool RmlUiMainMenu::consumeFontReloadRequest() {
    return false;
}

void RmlUiMainMenu::setConnectionState(const ConnectionState &state) {
    connectionState = state;
    if (communityPanel) {
        communityPanel->setConnectionState(state);
    }
}

MainMenuInterface::ConnectionState RmlUiMainMenu::getConnectionState() const {
    return connectionState;
}

bool RmlUiMainMenu::consumeQuitRequest() {
    if (!pendingQuitRequest) {
        return false;
    }
    pendingQuitRequest = false;
    return true;
}

void RmlUiMainMenu::showErrorDialog(const std::string &message) {
    if (communityPanel) {
        communityPanel->showErrorDialog(message);
    }
}

void RmlUiMainMenu::onCommunitySelection(int index) {
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

void RmlUiMainMenu::onCommunityAddRequested(const std::string &host) {
    if (host.empty()) {
        return;
    }
    pendingNewList = ServerListOption{std::string{}, host};
}

void RmlUiMainMenu::onCommunityAddCanceled() {
    if (communityPanel) {
        communityPanel->clearAddInput();
    }
}

void RmlUiMainMenu::onRefreshRequested() {
    pendingRefresh = true;
}

void RmlUiMainMenu::onServerSelection(int index) {
    if (index < 0 || index >= static_cast<int>(entries.size())) {
        return;
    }
    selectedServerIndex = index;
}

bool RmlUiMainMenu::loadUserConfig(nlohmann::json &out) const {
    const std::filesystem::path path = userConfigPath.empty()
        ? bz::data::EnsureUserConfigFile("config.json")
        : std::filesystem::path(userConfigPath);
    if (auto user = bz::data::LoadJsonFile(path, "user config", spdlog::level::debug)) {
        if (!user->is_object()) {
            out = nlohmann::json::object();
            return false;
        }
        out = *user;
        return true;
    }
    out = nlohmann::json::object();
    return true;
}


std::string RmlUiMainMenu::communityKeyForIndex(int index) const {
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

void RmlUiMainMenu::refreshCommunityCredentials() {
    if (listSelectedIndex == lastCredentialsListIndex) {
        return;
    }
    lastCredentialsListIndex = listSelectedIndex;
    if (communityPanel) {
        communityPanel->refreshCommunityCredentials();
    }
}


void RmlUiMainMenu::onJoinRequested(int index) {
    if (index < 0 || index >= static_cast<int>(entries.size())) {
        spdlog::warn("RmlUi MainMenu: Join requested with invalid index {}", index);
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
    spdlog::info("RmlUi MainMenu: Join queued host={} port={} sourceHost={} worldName={}",
                 entry.host, entry.port, entry.sourceHost, entry.worldName);
}

void RmlUiMainMenu::onQuitRequested() {
    pendingQuitRequest = true;
}

void RmlUiMainMenu::applyListOptionsToPanel() {
    if (!communityPanel) {
        return;
    }
    communityPanel->setListOptions(listOptions, listSelectedIndex);
}

} // namespace gui
