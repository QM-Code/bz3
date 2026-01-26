#include "ui/backends/imgui/console/console.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <imgui.h>
#include <imgui_internal.h>
#if defined(IMGUI_ENABLE_FREETYPE) && __has_include("imgui_freetype.h")
#include <imgui_freetype.h>
#endif
#include <spdlog/spdlog.h>
#include <string>
#include <vector>

#include "common/config_helpers.hpp"
#include "common/data_path_resolver.hpp"
#include "common/i18n.hpp"

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

namespace ui {

void ConsoleView::initializeFonts(ImGuiIO &io) {
    const ImVec4 defaultTextColor = ImGui::GetStyle().Colors[ImGuiCol_Text];
#if defined(IMGUI_ENABLE_FREETYPE) && defined(ImGuiFreeTypeBuilderFlags_LoadColor)
    io.Fonts->FontBuilderFlags |= ImGuiFreeTypeBuilderFlags_LoadColor;
#endif
    auto addFallbackFont = [&](const char *assetKey, float size, const ImWchar *ranges, const char *label) {
        const auto fontPath = bz::data::ResolveConfiguredAsset(assetKey);
        if (fontPath.empty()) {
            return;
        }
        ImFontConfig config;
        config.MergeMode = true;
        config.PixelSnapH = true;
        ImFont *font = io.Fonts->AddFontFromFileTTF(fontPath.string().c_str(), size, &config, ranges);
        if (!font) {
            spdlog::warn("Failed to load fallback font {} ({}).", label, fontPath.string());
        }
    };
    auto addFallbacksForSize = [&](float size, const std::string &language) {
        ImFontGlyphRangesBuilder builder;
        builder.AddRanges(io.Fonts->GetGlyphRangesDefault());
        builder.AddRanges(io.Fonts->GetGlyphRangesCyrillic());
        ImVector<ImWchar> latinRanges;
        builder.BuildRanges(&latinRanges);
        addFallbackFont("hud.fonts.console.FallbackLatin.Font", size, latinRanges.Data, "FallbackLatin");
        if (language == "ar") {
            static const ImWchar arabicRanges[] = {
                0x0600, 0x06FF, 0x0750, 0x077F, 0x08A0, 0x08FF, 0xFB50, 0xFDFF, 0xFE70, 0xFEFF, 0
            };
            addFallbackFont("hud.fonts.console.FallbackArabic.Font", size, arabicRanges, "FallbackArabic");
        } else if (language == "hi") {
            static const ImWchar devanagariRanges[] = { 0x0900, 0x097F, 0 };
            addFallbackFont("hud.fonts.console.FallbackDevanagari.Font", size, devanagariRanges, "FallbackDevanagari");
        } else if (language == "jp") {
            addFallbackFont("hud.fonts.console.FallbackCJK_JP.Font", size, io.Fonts->GetGlyphRangesJapanese(), "FallbackCJK_JP");
        } else if (language == "ko") {
            addFallbackFont("hud.fonts.console.FallbackCJK_KR.Font", size, io.Fonts->GetGlyphRangesKorean(), "FallbackCJK_KR");
        } else if (language == "zh") {
            addFallbackFont("hud.fonts.console.FallbackCJK_SC.Font", size, io.Fonts->GetGlyphRangesChineseSimplifiedCommon(), "FallbackCJK_SC");
        }
    };
    const std::string language = bz::i18n::Get().language();
    const char *regularFontKey = "hud.fonts.console.Regular.Font";
    if (language == "ru") {
        regularFontKey = "hud.fonts.console.FallbackLatin.Font";
    } else if (language == "zh") {
        regularFontKey = "hud.fonts.console.FallbackCJK_SC.Font";
    } else if (language == "jp") {
        regularFontKey = "hud.fonts.console.FallbackCJK_JP.Font";
    } else if (language == "ko") {
        regularFontKey = "hud.fonts.console.FallbackCJK_KR.Font";
    } else if (language == "ar") {
        regularFontKey = "hud.fonts.console.FallbackArabic.Font";
    } else if (language == "hi") {
        regularFontKey = "hud.fonts.console.FallbackDevanagari.Font";
    }
    const auto regularFontPath = bz::data::ResolveConfiguredAsset(regularFontKey);
    const std::string regularFontPathStr = regularFontPath.string();
    const ImWchar *regularRanges = nullptr;
    if (language == "ru") {
        regularRanges = io.Fonts->GetGlyphRangesCyrillic();
    } else if (language == "ar") {
        static const ImWchar arabicRanges[] = {
            0x0600, 0x06FF, 0x0750, 0x077F, 0x08A0, 0x08FF, 0xFB50, 0xFDFF, 0xFE70, 0xFEFF, 0
        };
        regularRanges = arabicRanges;
    } else if (language == "hi") {
        static const ImWchar devanagariRanges[] = { 0x0900, 0x097F, 0 };
        regularRanges = devanagariRanges;
    } else if (language == "jp") {
        regularRanges = io.Fonts->GetGlyphRangesJapanese();
    } else if (language == "ko") {
        regularRanges = io.Fonts->GetGlyphRangesKorean();
    } else if (language == "zh") {
        regularRanges = io.Fonts->GetGlyphRangesChineseSimplifiedCommon();
    }
    const float regularFontSize = useThemeOverrides
        ? currentTheme.regular.size
        : bz::data::ReadFloatConfig({"assets.hud.fonts.console.Regular.Size"}, 20.0f);
    this->regularFontSize = regularFontSize;
    regularFont = io.Fonts->AddFontFromFileTTF(
        regularFontPathStr.c_str(),
        regularFontSize,
        nullptr,
        regularRanges
    );
    regularColor = readColorConfig("assets.hud.fonts.console.Regular.Color", defaultTextColor);
    if (regularFont) {
        addFallbacksForSize(regularFontSize, bz::i18n::Get().language());
    }

    if (!regularFont) {
        spdlog::warn("Failed to load console regular font for community browser ({}).", regularFontPathStr);
    }

    const auto emojiFontPath = bz::data::ResolveConfiguredAsset("hud.fonts.console.Emoji.Font");
    this->emojiFontSize = 0.0f;
    if (!emojiFontPath.empty()) {
        const std::string emojiFontPathStr = emojiFontPath.string();
        const float emojiFontSize = useThemeOverrides
            ? currentTheme.emoji.size
            : bz::data::ReadFloatConfig({"assets.hud.fonts.console.Emoji.Size"}, regularFontSize);
        this->emojiFontSize = emojiFontSize;
        ImFontConfig emojiConfig;
        emojiConfig.MergeMode = true;
        emojiConfig.PixelSnapH = true;
        emojiConfig.OversampleH = 1;
        emojiConfig.OversampleV = 1;
#if defined(IMGUI_ENABLE_FREETYPE) && defined(ImGuiFreeTypeBuilderFlags_LoadColor)
        emojiConfig.FontBuilderFlags |= ImGuiFreeTypeBuilderFlags_LoadColor;
#endif
#ifdef IMGUI_USE_WCHAR32
        static const ImWchar emojiRanges[] = { 0x1, 0x1FFFF, 0 };
#else
        static const ImWchar emojiRanges[] = { 0x1, 0xFFFF, 0 };
        spdlog::warn("Emoji font loaded without IMGUI_USE_WCHAR32; codepoints above U+FFFF will not render.");
#endif
        emojiFont = io.Fonts->AddFontFromFileTTF(
            emojiFontPathStr.c_str(),
            emojiFontSize,
            &emojiConfig,
            emojiRanges
        );
        if (!emojiFont) {
            spdlog::warn("Failed to load emoji font for community browser ({}).", emojiFontPathStr);
        }
    }

    const auto titleFontPath = bz::data::ResolveConfiguredAsset("hud.fonts.console.Title.Font");
    const std::string titleFontPathStr = titleFontPath.string();
    const float titleFontSize = useThemeOverrides
        ? currentTheme.title.size
        : bz::data::ReadFloatConfig({"assets.hud.fonts.console.Title.Size"}, 30.0f);
    this->titleFontSize = titleFontSize;
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
    this->headingFontSize = headingFontSize;
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

void ConsoleView::draw(ImGuiIO &io) {
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
    const float bgAlpha = connectionState.connected ? 0.95f : 1.0f;
    ImGui::SetNextWindowBgAlpha(bgAlpha);

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoMove;

    const ImGuiStyle &style = ImGui::GetStyle();
    if (!connectionState.connected) {
        const ImVec2 screenMin(0.0f, 0.0f);
        const ImVec2 screenMax(io.DisplaySize.x, io.DisplaySize.y);
        ImVec4 bg = style.Colors[ImGuiCol_WindowBg];
        bg.w = 1.0f;
        ImGui::GetBackgroundDrawList()->AddRectFilled(screenMin, screenMax,
                                                      ImGui::GetColorU32(bg));
    }
    ImFont *titleFontToUse = titleFont ? titleFont : (headingFont ? headingFont : regularFont);
    if (titleFontToUse) {
        ImGui::PushFont(titleFontToUse);
    }
    ImGui::PushStyleColor(ImGuiCol_Text, titleColor);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
                        ImVec2(style.FramePadding.x + 6.0f, style.FramePadding.y + 4.0f));
    auto &i18n = bz::i18n::Get();
    const std::string windowTitle = i18n.get("ui.console.title");
    ImGui::Begin((windowTitle + "###MainConsole").c_str(), nullptr, flags);
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
    if (titleFontToUse) {
        ImGui::PopFont();
    }

    const MessageColors messageColors = getMessageColors();
    if (ImGui::BeginTabBar("CommunityBrowserTabs", ImGuiTabBarFlags_FittingPolicyScroll)) {
        const std::string tabCommunity = i18n.get("ui.console.tabs.community");
        const std::string tabSettings = i18n.get("ui.console.tabs.settings");
        const std::string tabDocumentation = i18n.get("ui.console.tabs.documentation");
        const std::string tabStartServer = i18n.get("ui.console.tabs.start_server");
        const std::string tabThemes = i18n.get("ui.console.tabs.themes");
        if (ImGui::BeginTabItem((tabCommunity + "###TabCommunity").c_str())) {
            drawCommunityPanel(messageColors);
            ImGui::EndTabItem();
        }
    if (ImGui::BeginTabItem((tabSettings + "###TabSettings").c_str())) {
        drawSettingsPanel(messageColors);
        ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem((tabDocumentation + "###TabDocumentation").c_str())) {
        drawDocumentationPanel(messageColors);
        ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem((tabStartServer + "###TabStartServer").c_str())) {
        drawStartServerPanel(messageColors);
        ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem((tabThemes + "###TabThemes").c_str())) {
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

void ConsoleView::setUserConfigPath(const std::string &path) {
    userConfigPath = path;
    themesLoaded = false;
    settingsLoaded = false;
    renderSettings.reset();
    if (!userConfigPath.empty()) {
        bz::json::Value userConfig;
        if (loadUserConfig(userConfig)) {
            renderSettings.load(userConfig);
        }
    }
}

void ConsoleView::setLanguageCallback(std::function<void(const std::string &)> callback) {
    languageCallback = std::move(callback);
}

bool ConsoleView::consumeFontReloadRequest() {
    const bool requested = fontReloadRequested;
    fontReloadRequested = false;
    return requested;
}

bool ConsoleView::consumeKeybindingsReloadRequest() {
    const bool requested = keybindingsReloadRequested;
    keybindingsReloadRequested = false;
    return requested;
}

void ConsoleView::requestKeybindingsReload() {
    if (!userConfigPath.empty()) {
        bz::data::MergeExternalConfigLayer(userConfigPath, "user config", spdlog::level::debug);
    }
    keybindingsReloadRequested = true;
}

void ConsoleView::setConnectionState(const ConnectionState &state) {
    connectionState = state;
}

ConsoleInterface::ConnectionState ConsoleView::getConnectionState() const {
    return connectionState;
}

bool ConsoleView::consumeQuitRequest() {
    if (!pendingQuitRequest) {
        return false;
    }
    pendingQuitRequest = false;
    return true;
}

void ConsoleView::showErrorDialog(const std::string &message) {
    errorDialogMessage = message;
}

void ConsoleView::drawPlaceholderPanel(const char *heading,
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

void ConsoleView::show(const std::vector<CommunityBrowserEntry> &newEntries) {
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
    serverLinkStatusText.clear();
    serverLinkStatusIsError = false;
    serverDescriptionLoadingKey.clear();
    serverDescriptionLoading = false;
    serverDescriptionErrorKey.clear();
    serverDescriptionErrorText.clear();
    communityStatusTone = MessageTone::Notice;
    clearPassword();
    showNewCommunityInput = false;
    listUrlBuffer.fill(0);
}

ConsoleView::~ConsoleView() {
    stopAllLocalServers();
    thumbnails.shutdown();
}

void ConsoleView::setEntries(const std::vector<CommunityBrowserEntry> &newEntries) {
    entries = newEntries;
    if (entries.empty()) {
        selectedIndex = -1;
    } else if (selectedIndex < 0) {
        selectedIndex = 0;
    } else if (selectedIndex >= static_cast<int>(entries.size())) {
        selectedIndex = static_cast<int>(entries.size()) - 1;
    }
}

void ConsoleView::setListOptions(const std::vector<ServerListOption> &options, int selectedIndexIn) {
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

std::string ConsoleView::communityKeyForIndex(int index) const {
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

void ConsoleView::refreshCommunityCredentials() {
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

    bz::json::Value config;
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

void ConsoleView::persistCommunityCredentials(bool passwordChanged) {
    const std::string key = communityKeyForIndex(listSelectedIndex);
    if (key.empty()) {
        return;
    }

    bz::json::Value config;
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

void ConsoleView::storeCommunityAuth(const std::string &communityHost,
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

    bz::json::Value config;
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

void ConsoleView::hide() {
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
    serverLinkStatusText.clear();
    serverLinkStatusIsError = false;
    serverDescriptionLoadingKey.clear();
    serverDescriptionLoading = false;
    serverDescriptionErrorKey.clear();
    serverDescriptionErrorText.clear();
    communityStatusTone = MessageTone::Notice;
    clearPassword();
    showNewCommunityInput = false;
    thumbnails.shutdown();
}

bool ConsoleView::isVisible() const {
    return visible;
}

void ConsoleView::setStatus(const std::string &text, bool isErrorMessage) {
    statusText = text;
    statusIsError = isErrorMessage;
}

void ConsoleView::setCommunityDetails(const std::string &detailsText) {
    communityDetailsText = detailsText;
}

void ConsoleView::setServerDescriptionLoading(const std::string &key, bool loading) {
    serverDescriptionLoadingKey = key;
    serverDescriptionLoading = loading;
}

bool ConsoleView::isServerDescriptionLoading(const std::string &key) const {
    if (!serverDescriptionLoading || key.empty()) {
        return false;
    }
    return serverDescriptionLoadingKey == key;
}

void ConsoleView::setServerDescriptionError(const std::string &key, const std::string &message) {
    serverDescriptionErrorKey = key;
    serverDescriptionErrorText = message;
}

std::optional<std::string> ConsoleView::getServerDescriptionError(const std::string &key) const {
    if (key.empty() || serverDescriptionErrorKey.empty()) {
        return std::nullopt;
    }
    if (serverDescriptionErrorKey != key) {
        return std::nullopt;
    }
    if (serverDescriptionErrorText.empty()) {
        return std::nullopt;
    }
    return serverDescriptionErrorText;
}

std::optional<CommunityBrowserSelection> ConsoleView::consumeSelection() {
    if (!pendingSelection.has_value()) {
        return std::nullopt;
    }

    auto selection = pendingSelection;
    pendingSelection.reset();
    return selection;
}

std::optional<int> ConsoleView::consumeListSelection() {
    if (!pendingListSelection.has_value()) {
        return std::nullopt;
    }

    auto selection = pendingListSelection;
    pendingListSelection.reset();
    return selection;
}

std::optional<ServerListOption> ConsoleView::consumeNewListRequest() {
    if (!pendingNewList.has_value()) {
        return std::nullopt;
    }

    auto request = pendingNewList;
    pendingNewList.reset();
    return request;
}

std::optional<std::string> ConsoleView::consumeDeleteListRequest() {
    if (!pendingDeleteListHost.has_value()) {
        return std::nullopt;
    }

    auto host = pendingDeleteListHost;
    pendingDeleteListHost.reset();
    return host;
}

void ConsoleView::setListStatus(const std::string &text, bool isErrorMessage) {
    listStatusText = text;
    listStatusIsError = isErrorMessage;
}

void ConsoleView::clearNewListInputs() {
    listUrlBuffer.fill(0);
}

void ConsoleView::setCommunityStatus(const std::string &text, MessageTone tone) {
    communityStatusText = text;
    communityStatusTone = tone;
}

std::optional<CommunityBrowserEntry> ConsoleView::getSelectedEntry() const {
    if (selectedIndex < 0 || selectedIndex >= static_cast<int>(entries.size())) {
        return std::nullopt;
    }
    return entries[static_cast<std::size_t>(selectedIndex)];
}

std::string ConsoleView::getUsername() const {
    return trimCopy(usernameBuffer.data());
}

std::string ConsoleView::getPassword() const {
    return std::string(passwordBuffer.data());
}

std::string ConsoleView::getStoredPasswordHash() const {
    return storedPasswordHash;
}

void ConsoleView::clearPassword() {
    passwordBuffer.fill(0);
}

bool ConsoleView::consumeRefreshRequest() {
    if (!refreshRequested) {
        return false;
    }
    refreshRequested = false;
    return true;
}

void ConsoleView::setScanning(bool isScanning) {
    scanning = isScanning;
}

ThumbnailTexture *ConsoleView::getOrLoadThumbnail(const std::string &url) {
    return thumbnails.getOrLoad(url);
}

ConsoleView::MessageColors ConsoleView::getMessageColors() const {
    MessageColors colors;
    colors.error = ImVec4(0.93f, 0.36f, 0.36f, 1.0f);
    colors.notice = ImVec4(0.90f, 0.80f, 0.30f, 1.0f);
    colors.action = ImVec4(0.60f, 0.80f, 0.40f, 1.0f);
    colors.pending = ImVec4(0.35f, 0.70f, 0.95f, 1.0f);
    return colors;
}

} // namespace ui
