#include "engine/components/gui/main_menu.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <imgui.h>
#include <imgui_internal.h>
#include <spdlog/spdlog.h>
#include <string>
#include <vector>

#include "common/config_helpers.hpp"
#include "common/data_path_resolver.hpp"

namespace {
std::string trimCopy(const std::string &value) {
    auto begin = std::find_if_not(value.begin(), value.end(), [](unsigned char ch) {
        return std::isspace(ch) != 0;
    });
    auto end = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char ch) {
        return std::isspace(ch) != 0;
    }).base();

    if (begin >= end) {
        return {};
    }

    return std::string(begin, end);
}

ImVec4 readColorConfig(const char *path, const ImVec4 &fallback) {
    const auto *value = bz::data::ConfigValue(path);
    if (!value || !value->is_array()) {
        return fallback;
    }
    const auto &arr = *value;
    if (arr.size() < 3 || arr.size() > 4) {
        return fallback;
    }
    auto readComponent = [&](std::size_t index, float defaultValue) -> float {
        if (index >= arr.size() || !arr[index].is_number()) {
            return defaultValue;
        }
        return static_cast<float>(arr[index].get<double>());
    };
    ImVec4 color = fallback;
    color.x = readComponent(0, color.x);
    color.y = readComponent(1, color.y);
    color.z = readComponent(2, color.z);
    if (arr.size() >= 4) {
        color.w = readComponent(3, color.w);
    }
    return color;
}

}

namespace gui {

void MainMenuView::initializeFonts(ImGuiIO &io) {
    const ImVec4 defaultTextColor = ImGui::GetStyle().Colors[ImGuiCol_Text];
    const auto regularFontPath = bz::data::ResolveConfiguredAsset("hud.fonts.console.Regular.Font");
    const std::string regularFontPathStr = regularFontPath.string();
    const float regularFontSize = useThemeOverrides
        ? currentTheme.regular.size
        : bz::data::ReadFloatConfig({"assets.hud.fonts.console.Regular.Size"}, 20.0f);
    regularFont = io.Fonts->AddFontFromFileTTF(
        regularFontPathStr.c_str(),
        regularFontSize
    );
    regularColor = readColorConfig("assets.hud.fonts.console.Regular.Color", defaultTextColor);

    if (!regularFont) {
        spdlog::warn("Failed to load console regular font for community browser ({}).", regularFontPathStr);
    }

    const auto titleFontPath = bz::data::ResolveConfiguredAsset("hud.fonts.console.Title.Font");
    const std::string titleFontPathStr = titleFontPath.string();
    const float titleFontSize = useThemeOverrides
        ? currentTheme.title.size
        : bz::data::ReadFloatConfig({"assets.hud.fonts.console.Title.Size"}, 30.0f);
    titleFont = io.Fonts->AddFontFromFileTTF(
        titleFontPathStr.c_str(),
        titleFontSize
    );
    titleColor = readColorConfig("assets.hud.fonts.console.Title.Color", defaultTextColor);

    if (!titleFont) {
        spdlog::warn("Failed to load console title font for community browser ({}).", titleFontPathStr);
    }

    const auto headingFontPath = bz::data::ResolveConfiguredAsset("hud.fonts.console.Heading.Font");
    const std::string headingFontPathStr = headingFontPath.string();
    const float headingFontSize = useThemeOverrides
        ? currentTheme.heading.size
        : bz::data::ReadFloatConfig({"assets.hud.fonts.console.Heading.Size"}, 28.0f);
    headingFont = io.Fonts->AddFontFromFileTTF(
        headingFontPathStr.c_str(),
        headingFontSize
    );
    headingColor = readColorConfig("assets.hud.fonts.console.Heading.Color", defaultTextColor);

    if (!headingFont) {
        spdlog::warn("Failed to load console heading font for community browser ({}).", headingFontPathStr);
    }

    const auto buttonFontPath = bz::data::ResolveConfiguredAsset("hud.fonts.console.Button.Font");
    const std::string buttonFontPathStr = buttonFontPath.string();
    const float buttonFontSize = useThemeOverrides
        ? currentTheme.button.size
        : bz::data::ReadFloatConfig({"assets.hud.fonts.console.Button.Size"}, 18.0f);
    buttonFont = io.Fonts->AddFontFromFileTTF(
        buttonFontPathStr.c_str(),
        buttonFontSize
    );
    buttonColor = readColorConfig("assets.hud.fonts.console.Button.Color", defaultTextColor);

    if (!buttonFont) {
        spdlog::warn("Failed to load console button font for community browser ({}).", buttonFontPathStr);
    }
}

void MainMenuView::draw(ImGuiIO &io) {
    if (!visible) {
        return;
    }

    thumbnails.processUploads();

    const bool pushedRegularFont = (regularFont != nullptr);
    if (pushedRegularFont) {
        ImGui::PushFont(regularFont);
    }
    ImGui::PushStyleColor(ImGuiCol_Text, regularColor);
    const bool pushedRegularColor = true;

    const bool hasHeadingFont = (headingFont != nullptr);
    const bool hasButtonFont = (buttonFont != nullptr);

    const ImVec2 windowSize(1200.0f, 680.0f);
    const ImVec2 windowPos(
        (io.DisplaySize.x - windowSize.x) * 0.5f,
        (io.DisplaySize.y - windowSize.y) * 0.5f
    );

    ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(windowSize, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.95f);

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoMove;

    const ImGuiStyle &style = ImGui::GetStyle();
    ImFont *titleFontToUse = titleFont ? titleFont : (headingFont ? headingFont : regularFont);
    if (titleFontToUse) {
        ImGui::PushFont(titleFontToUse);
    }
    ImGui::PushStyleColor(ImGuiCol_Text, titleColor);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
                        ImVec2(style.FramePadding.x + 6.0f, style.FramePadding.y + 4.0f));
    ImGui::Begin("BZ3 - BZFlag Revisited", nullptr, flags);
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
    if (titleFontToUse) {
        ImGui::PopFont();
    }

    const MessageColors messageColors = getMessageColors();
    if (ImGui::BeginTabBar("CommunityBrowserTabs", ImGuiTabBarFlags_FittingPolicyScroll)) {
        if (ImGui::BeginTabItem("Community")) {
            drawCommunityPanel(messageColors);
            ImGui::EndTabItem();
        }
    if (ImGui::BeginTabItem("Settings")) {
        drawSettingsPanel(messageColors);
        ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Documentation")) {
        drawDocumentationPanel(messageColors);
        ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Start Server")) {
        drawStartServerPanel(messageColors);
        ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Themes")) {
        drawThemesPanel(messageColors);
        ImGui::EndTabItem();
    }
    ImGui::EndTabBar();
    }

    ImGui::End();

    if (pushedRegularColor) {
        ImGui::PopStyleColor();
    }
    if (pushedRegularFont) {
        ImGui::PopFont();
    }
}

void MainMenuView::setUserConfigPath(const std::string &path) {
    userConfigPath = path;
    themesLoaded = false;
    settingsLoaded = false;
}

bool MainMenuView::consumeFontReloadRequest() {
    const bool requested = fontReloadRequested;
    fontReloadRequested = false;
    return requested;
}

void MainMenuView::drawPlaceholderPanel(const char *heading,
                                                const char *body,
                                                const MessageColors &colors) const {
    if (headingFont) {
        ImGui::PushFont(headingFont);
    }
    ImGui::PushStyleColor(ImGuiCol_Text, headingColor);
    ImGui::TextUnformatted(heading);
    ImGui::PopStyleColor();
    if (headingFont) {
        ImGui::PopFont();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::PushStyleColor(ImGuiCol_Text, colors.notice);
    ImGui::TextWrapped("%s", body);
    ImGui::PopStyleColor();
}

void MainMenuView::show(const std::vector<CommunityBrowserEntry> &newEntries) {
    visible = true;
    setEntries(newEntries);
    pendingSelection.reset();
    statusText = "Select a server to connect.";
    statusIsError = false;
    pendingListSelection.reset();
    pendingNewList.reset();
    pendingDeleteListHost.reset();
    listStatusText.clear();
    listStatusIsError = false;
    communityStatusText.clear();
    communityDetailsText.clear();
    communityLinkStatusText.clear();
    communityLinkStatusIsError = false;
    communityStatusTone = MessageTone::Notice;
    clearPassword();
    showNewCommunityInput = false;
    listUrlBuffer.fill(0);
}

MainMenuView::~MainMenuView() {
    stopAllLocalServers();
    thumbnails.shutdown();
}

void MainMenuView::setEntries(const std::vector<CommunityBrowserEntry> &newEntries) {
    entries = newEntries;
    if (entries.empty()) {
        selectedIndex = -1;
    } else if (selectedIndex < 0) {
        selectedIndex = 0;
    } else if (selectedIndex >= static_cast<int>(entries.size())) {
        selectedIndex = static_cast<int>(entries.size()) - 1;
    }
}

void MainMenuView::setListOptions(const std::vector<ServerListOption> &options, int selectedIndexIn) {
    listOptions = options;
    if (listOptions.empty()) {
        listSelectedIndex = -1;
        serverCommunityIndex = -1;
        lastCredentialsListIndex = -1;
        pendingListSelection.reset();
        return;
    }

    if (selectedIndexIn < 0) {
        listSelectedIndex = 0;
    } else if (selectedIndexIn >= static_cast<int>(listOptions.size())) {
        listSelectedIndex = static_cast<int>(listOptions.size()) - 1;
    } else {
        listSelectedIndex = selectedIndexIn;
    }

    if (serverCommunityIndex < 0 || serverCommunityIndex >= static_cast<int>(listOptions.size())) {
        serverCommunityIndex = listSelectedIndex;
    }
}

std::string MainMenuView::communityKeyForIndex(int index) const {
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

void MainMenuView::refreshCommunityCredentials() {
    if (listSelectedIndex == lastCredentialsListIndex) {
        return;
    }
    lastCredentialsListIndex = listSelectedIndex;
    usernameBuffer.fill(0);
    passwordBuffer.fill(0);
    storedPasswordHash.clear();

    const std::string key = communityKeyForIndex(listSelectedIndex);
    if (key.empty()) {
        return;
    }

    nlohmann::json config;
    if (!loadUserConfig(config)) {
        return;
    }

    const auto guiIt = config.find("gui");
    if (guiIt == config.end() || !guiIt->is_object()) {
        return;
    }
    const auto credsIt = guiIt->find("communityCredentials");
    if (credsIt == guiIt->end() || !credsIt->is_object()) {
        return;
    }
    const auto entryIt = credsIt->find(key);
    if (entryIt == credsIt->end() || !entryIt->is_object()) {
        return;
    }
    const auto &entry = *entryIt;
    if (auto userIt = entry.find("username"); userIt != entry.end() && userIt->is_string()) {
        std::snprintf(usernameBuffer.data(), usernameBuffer.size(), "%s", userIt->get<std::string>().c_str());
    }
    if (key != "LAN") {
        if (auto passIt = entry.find("passwordHash"); passIt != entry.end() && passIt->is_string()) {
            const std::string passhash = passIt->get<std::string>();
            if (!passhash.empty()) {
                storedPasswordHash = passhash;
            }
        }
    }
}

void MainMenuView::persistCommunityCredentials(bool passwordChanged) {
    const std::string key = communityKeyForIndex(listSelectedIndex);
    if (key.empty()) {
        return;
    }

    nlohmann::json config;
    if (!loadUserConfig(config)) {
        return;
    }

    const std::string username = trimCopy(usernameBuffer.data());
    if (username.empty()) {
        eraseNestedConfig(config, {"gui", "communityCredentials", key.c_str()});
    } else {
        setNestedConfig(config, {"gui", "communityCredentials", key.c_str(), "username"}, username);
        if (key == "LAN") {
            eraseNestedConfig(config, {"gui", "communityCredentials", key.c_str(), "passwordHash"});
            eraseNestedConfig(config, {"gui", "communityCredentials", key.c_str(), "salt"});
        } else if (!storedPasswordHash.empty()) {
            setNestedConfig(config, {"gui", "communityCredentials", key.c_str(), "passwordHash"}, storedPasswordHash);
        } else if (passwordChanged) {
            eraseNestedConfig(config, {"gui", "communityCredentials", key.c_str(), "passwordHash"});
        }
    }

    std::string error;
    saveUserConfig(config, error);
}

void MainMenuView::storeCommunityAuth(const std::string &communityHost,
                                      const std::string &username,
                                      const std::string &passhash,
                                      const std::string &salt) {
    if (communityHost.empty() || username.empty()) {
        return;
    }

    std::string key = communityHost;
    while (!key.empty() && key.back() == '/') {
        key.pop_back();
    }

    nlohmann::json config;
    if (!loadUserConfig(config)) {
        return;
    }

    setNestedConfig(config, {"gui", "communityCredentials", key.c_str(), "username"}, username);
    if (!passhash.empty()) {
        setNestedConfig(config, {"gui", "communityCredentials", key.c_str(), "passwordHash"}, passhash);
    }
    if (!salt.empty()) {
        setNestedConfig(config, {"gui", "communityCredentials", key.c_str(), "salt"}, salt);
    }

    std::string error;
    saveUserConfig(config, error);

    const std::string activeKey = communityKeyForIndex(listSelectedIndex);
    if (activeKey == key) {
        std::snprintf(usernameBuffer.data(), usernameBuffer.size(), "%s", username.c_str());
        if (!passhash.empty()) {
            storedPasswordHash = passhash;
        }
    }
}

void MainMenuView::hide() {
    visible = false;
    statusText.clear();
    statusIsError = false;
    pendingSelection.reset();
    pendingListSelection.reset();
    pendingNewList.reset();
    pendingDeleteListHost.reset();
    refreshRequested = false;
    scanning = false;
    listStatusText.clear();
    listStatusIsError = false;
    communityStatusText.clear();
    communityDetailsText.clear();
    communityLinkStatusText.clear();
    communityLinkStatusIsError = false;
    communityStatusTone = MessageTone::Notice;
    clearPassword();
    showNewCommunityInput = false;
    thumbnails.shutdown();
}

bool MainMenuView::isVisible() const {
    return visible;
}

void MainMenuView::setStatus(const std::string &text, bool isErrorMessage) {
    statusText = text;
    statusIsError = isErrorMessage;
}

void MainMenuView::setCommunityDetails(const std::string &detailsText) {
    communityDetailsText = detailsText;
}

std::optional<CommunityBrowserSelection> MainMenuView::consumeSelection() {
    if (!pendingSelection.has_value()) {
        return std::nullopt;
    }

    auto selection = pendingSelection;
    pendingSelection.reset();
    return selection;
}

std::optional<int> MainMenuView::consumeListSelection() {
    if (!pendingListSelection.has_value()) {
        return std::nullopt;
    }

    auto selection = pendingListSelection;
    pendingListSelection.reset();
    return selection;
}

std::optional<ServerListOption> MainMenuView::consumeNewListRequest() {
    if (!pendingNewList.has_value()) {
        return std::nullopt;
    }

    auto request = pendingNewList;
    pendingNewList.reset();
    return request;
}

std::optional<std::string> MainMenuView::consumeDeleteListRequest() {
    if (!pendingDeleteListHost.has_value()) {
        return std::nullopt;
    }

    auto host = pendingDeleteListHost;
    pendingDeleteListHost.reset();
    return host;
}

void MainMenuView::setListStatus(const std::string &text, bool isErrorMessage) {
    listStatusText = text;
    listStatusIsError = isErrorMessage;
}

void MainMenuView::clearNewListInputs() {
    listUrlBuffer.fill(0);
}

void MainMenuView::setCommunityStatus(const std::string &text, MessageTone tone) {
    communityStatusText = text;
    communityStatusTone = tone;
}

std::string MainMenuView::getUsername() const {
    return trimCopy(usernameBuffer.data());
}

std::string MainMenuView::getPassword() const {
    return std::string(passwordBuffer.data());
}

std::string MainMenuView::getStoredPasswordHash() const {
    return storedPasswordHash;
}

void MainMenuView::clearPassword() {
    passwordBuffer.fill(0);
}

bool MainMenuView::consumeRefreshRequest() {
    if (!refreshRequested) {
        return false;
    }
    refreshRequested = false;
    return true;
}

void MainMenuView::setScanning(bool isScanning) {
    scanning = isScanning;
}

ThumbnailTexture *MainMenuView::getOrLoadThumbnail(const std::string &url) {
    return thumbnails.getOrLoad(url);
}

MainMenuView::MessageColors MainMenuView::getMessageColors() const {
    MessageColors colors;
    colors.error = ImVec4(0.93f, 0.36f, 0.36f, 1.0f);
    colors.notice = ImVec4(0.90f, 0.80f, 0.30f, 1.0f);
    colors.action = ImVec4(0.60f, 0.80f, 0.40f, 1.0f);
    colors.pending = ImVec4(0.35f, 0.70f, 0.95f, 1.0f);
    return colors;
}

} // namespace gui
