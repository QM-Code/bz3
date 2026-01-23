#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>
#include <unordered_map>

struct ImGuiIO;
struct ImFont;

#include <imgui.h>
#include <nlohmann/json.hpp>

#include "ui/console/console_interface.hpp"
#include "ui/backends/imgui/console/thumbnail_cache.hpp"

namespace ui {

class ConsoleView : public ConsoleInterface {
public:
    using MessageTone = ui::MessageTone;

    struct ThemeFontConfig {
        std::string font;
        float size = 0.0f;
        ImVec4 color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    };

    struct ThemeConfig {
        std::string name;
    ThemeFontConfig regular;
    ThemeFontConfig emoji;
    ThemeFontConfig title;
    ThemeFontConfig heading;
    ThemeFontConfig button;
    };

    ~ConsoleView();
    void initializeFonts(ImGuiIO &io);
    void draw(ImGuiIO &io);

    void show(const std::vector<CommunityBrowserEntry> &entries) override;
    void setEntries(const std::vector<CommunityBrowserEntry> &entries) override;
    void setListOptions(const std::vector<ServerListOption> &options, int selectedIndex) override;
    void hide() override;
    bool isVisible() const override;
    void setStatus(const std::string &statusText, bool isErrorMessage) override;
    void setCommunityDetails(const std::string &detailsText) override;
    void setServerDescriptionLoading(const std::string &key, bool loading) override;
    bool isServerDescriptionLoading(const std::string &key) const override;
    void setServerDescriptionError(const std::string &key, const std::string &message) override;
    std::optional<std::string> getServerDescriptionError(const std::string &key) const override;
    std::optional<CommunityBrowserSelection> consumeSelection() override;
    std::optional<int> consumeListSelection() override;
    std::optional<ServerListOption> consumeNewListRequest() override;
    std::optional<std::string> consumeDeleteListRequest() override;
    void setListStatus(const std::string &statusText, bool isErrorMessage) override;
    void clearNewListInputs() override;
    std::string getUsername() const override;
    std::string getPassword() const override;
    std::string getStoredPasswordHash() const override;
    void clearPassword() override;
    void storeCommunityAuth(const std::string &communityHost,
                            const std::string &username,
                            const std::string &passhash,
                            const std::string &salt) override;
    void setCommunityStatus(const std::string &text, MessageTone tone) override;
    std::optional<CommunityBrowserEntry> getSelectedEntry() const override;
    bool consumeRefreshRequest() override;
    void setScanning(bool scanning) override;
    void setUserConfigPath(const std::string &path) override;
    bool consumeFontReloadRequest() override;
    void setConnectionState(const ConnectionState &state) override;
    ConnectionState getConnectionState() const override;
    bool consumeQuitRequest() override;
    void showErrorDialog(const std::string &message) override;

private:
    struct MessageColors {
        ImVec4 error;
        ImVec4 notice;
        ImVec4 action;
        ImVec4 pending;
    };

    struct LocalServerProcess;

    ThumbnailTexture *getOrLoadThumbnail(const std::string &url);
    MessageColors getMessageColors() const;
    void drawSettingsPanel(const MessageColors &colors);
    void drawDocumentationPanel(const MessageColors &colors) const;
    void drawStartServerPanel(const MessageColors &colors);
    void drawPlaceholderPanel(const char *heading, const char *body, const MessageColors &colors) const;
    void drawCommunityPanel(const MessageColors &colors);
    void drawThemesPanel(const MessageColors &colors);
    void ensureThemesLoaded();
    void applyThemeToView(const ThemeConfig &theme);
    bool loadUserConfig(nlohmann::json &out) const;
    bool saveUserConfig(const nlohmann::json &userConfig, std::string &error) const;
    void setNestedConfig(nlohmann::json &root, std::initializer_list<const char*> path, nlohmann::json value) const;
    void setNestedConfig(nlohmann::json &root, const std::vector<std::string> &path, nlohmann::json value) const;
    void eraseNestedConfig(nlohmann::json &root, std::initializer_list<const char*> path) const;
    std::string communityKeyForIndex(int index) const;
    void refreshCommunityCredentials();
    void persistCommunityCredentials(bool passwordChanged);
    nlohmann::json themeToJson(const ThemeConfig &theme) const;
    ThemeConfig themeFromJson(const nlohmann::json &themeJson, const ThemeConfig &fallback) const;
    void applyThemeSelection(const std::string &name);
    void resetToDefaultTheme();
    void stopAllLocalServers();
    void stopLocalServer(std::size_t index);
    std::string findServerBinary();
    bool startLocalServer(uint16_t port,
                          const std::string &worldDir,
                          bool useDefaultWorld,
                          const std::string &advertiseHost,
                          const std::string &communityUrl,
                          const std::string &communityLabel,
                          const std::string &logLevel,
                          std::string &error);
    bool launchLocalServer(LocalServerProcess &server, std::string &error);
    bool isPortInUse(uint16_t port, int ignoreId) const;

    bool visible = false;
    ImFont *regularFont = nullptr;
    ImFont *emojiFont = nullptr;
    ImFont *titleFont = nullptr;
    ImFont *headingFont = nullptr;
    ImFont *buttonFont = nullptr;
    ImVec4 regularColor{};
    ImVec4 titleColor{};
    ImVec4 headingColor{};
    ImVec4 buttonColor{};
    float regularFontSize = 0.0f;
    float emojiFontSize = 0.0f;
    float titleFontSize = 0.0f;
    float headingFontSize = 0.0f;
    bool fontReloadRequested = false;

    std::vector<CommunityBrowserEntry> entries;
    int selectedIndex = -1;
    std::array<char, 64> usernameBuffer{};
    std::array<char, 128> passwordBuffer{};
    std::string statusText;
    bool statusIsError = false;
    std::optional<CommunityBrowserSelection> pendingSelection;

    std::vector<ServerListOption> listOptions;
    int listSelectedIndex = -1;
    std::optional<int> pendingListSelection;
    std::optional<ServerListOption> pendingNewList;
    std::optional<std::string> pendingDeleteListHost;
    bool refreshRequested = false;
    bool scanning = false;
    bool showNewCommunityInput = false;
    std::array<char, 512> listUrlBuffer{};
    std::string listStatusText;
    bool listStatusIsError = false;
    std::string communityStatusText;
    std::string communityDetailsText;
    std::string communityLinkStatusText;
    bool communityLinkStatusIsError = false;
    std::string serverLinkStatusText;
    bool serverLinkStatusIsError = false;
    std::string serverDescriptionLoadingKey;
    bool serverDescriptionLoading = false;
    std::string serverDescriptionErrorKey;
    std::string serverDescriptionErrorText;
    MessageTone communityStatusTone = MessageTone::Notice;
    int lastCredentialsListIndex = -1;
    std::string storedPasswordHash;
    ConnectionState connectionState{};
    bool pendingQuitRequest = false;
    std::string errorDialogMessage;

    ThumbnailCache thumbnails;

    std::string userConfigPath;
    bool themesLoaded = false;
    std::vector<std::string> themeOptions;
    std::unordered_map<std::string, ThemeConfig> themePresets;
    std::optional<ThemeConfig> customTheme;
    ThemeConfig defaultTheme;
    ThemeConfig currentTheme;
    int selectedThemeIndex = 0;
    std::array<char, 64> themeNameBuffer{};
    bool themeDirty = false;
    std::string themeStatusText;
    bool themeStatusIsError = false;
    bool useThemeOverrides = false;
    enum class BindingColumn {
        Keyboard,
        Mouse,
        Controller
    };
    static constexpr std::size_t kKeybindingCount = 11;
    std::array<std::array<char, 128>, kKeybindingCount> keybindingKeyboardBuffers{};
    std::array<std::array<char, 128>, kKeybindingCount> keybindingMouseBuffers{};
    std::array<std::array<char, 128>, kKeybindingCount> keybindingControllerBuffers{};
    int selectedBindingIndex = -1;
    BindingColumn selectedBindingColumn = BindingColumn::Keyboard;
    bool settingsLoaded = false;
    std::string settingsStatusText;
    bool settingsStatusIsError = false;

    struct LocalServerProcess {
        int id = 0;
        uint16_t port = 0;
        std::string worldDir;
        bool useDefaultWorld = false;
        std::string logLevel;
        std::string advertiseHost;
        std::string communityUrl;
        std::string communityLabel;
        std::string dataDir;
        std::string configPath;
        int pid = -1;
        int logFd = -1;
        std::thread logThread;
        std::mutex logMutex;
        std::string logBuffer;
        std::atomic<bool> running{false};
        int exitStatus = 0;
    };

    std::deque<std::unique_ptr<LocalServerProcess>> localServers;
    int nextLocalServerId = 1;
    int selectedLogServerId = -1;
    bool serverBinaryChecked = false;
    std::string serverBinaryPath;
    std::string serverStatusText;
    bool serverStatusIsError = false;
    std::array<char, 64> serverAdvertiseHostBuffer{};
    std::array<char, 128> serverWorldBuffer{};
    int serverPortInput = 11899;
    int serverLogLevelIndex = 2;
    int serverCommunityIndex = -1;
};

} // namespace ui
