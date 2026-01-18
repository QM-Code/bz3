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

#include "engine/components/gui/thumbnail_cache.hpp"

namespace gui {

struct CommunityBrowserEntry {
    std::string label;
    std::string host;
    uint16_t port;
    std::string description;
    std::string displayHost;
    std::string longDescription;
    std::vector<std::string> flags;
    int activePlayers = -1;
    int maxPlayers = -1;
    std::string gameMode;
    std::string screenshotId;
    std::string sourceHost;
    std::string worldName;
};

struct CommunityBrowserSelection {
    std::string host;
    uint16_t port;
    bool fromPreset;
    std::string sourceHost;
    std::string worldName;
};

struct ServerListOption {
    std::string name;
    std::string host;
};

class MainMenuView {
public:
    enum class MessageTone {
        Notice,
        Error,
        Pending
    };

    struct ThemeFontConfig {
        std::string font;
        float size = 0.0f;
        ImVec4 color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    };

    struct ThemeConfig {
        std::string name;
        ThemeFontConfig regular;
        ThemeFontConfig title;
        ThemeFontConfig heading;
        ThemeFontConfig button;
    };

    ~MainMenuView();
    void initializeFonts(ImGuiIO &io);
    void draw(ImGuiIO &io);

    void show(const std::vector<CommunityBrowserEntry> &entries,
              const std::string &defaultHost,
              uint16_t defaultPort);
    void setEntries(const std::vector<CommunityBrowserEntry> &entries);
    void setListOptions(const std::vector<ServerListOption> &options, int selectedIndex);
    void hide();
    bool isVisible() const;
    void setStatus(const std::string &statusText, bool isErrorMessage);
    void setCustomStatus(const std::string &statusText, bool isErrorMessage);
    std::optional<CommunityBrowserSelection> consumeSelection();
    std::optional<int> consumeListSelection();
    std::optional<ServerListOption> consumeNewListRequest();
    void setListStatus(const std::string &statusText, bool isErrorMessage);
    void clearNewListInputs();
    std::string getUsername() const;
    std::string getPassword() const;
    std::string getStoredPasswordHash() const;
    void clearPassword();
    void storeCommunityAuth(const std::string &communityHost,
                            const std::string &username,
                            const std::string &passhash,
                            const std::string &salt);
    void setCommunityStatus(const std::string &text, MessageTone tone);
    bool consumeRefreshRequest();
    void setScanning(bool scanning);
    void setUserConfigPath(const std::string &path);
    bool consumeFontReloadRequest();

private:
    struct MessageColors {
        ImVec4 error;
        ImVec4 notice;
        ImVec4 action;
        ImVec4 pending;
    };

    struct LocalServerProcess;

    void resetBuffers(const std::string &defaultHost, uint16_t defaultPort);
    ThumbnailTexture *getOrLoadThumbnail(const std::string &url);
    MessageColors getMessageColors() const;
    void drawSettingsPanel(const MessageColors &colors) const;
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
    ImFont *titleFont = nullptr;
    ImFont *headingFont = nullptr;
    ImFont *buttonFont = nullptr;
    ImVec4 regularColor{};
    ImVec4 titleColor{};
    ImVec4 headingColor{};
    ImVec4 buttonColor{};
    bool fontReloadRequested = false;

    std::vector<CommunityBrowserEntry> entries;
    int selectedIndex = -1;
    std::array<char, 256> addressBuffer{};
    std::array<char, 64> usernameBuffer{};
    std::array<char, 128> passwordBuffer{};
    std::string statusText;
    bool statusIsError = false;
    std::string customStatusText;
    bool customStatusIsError = false;
    std::optional<CommunityBrowserSelection> pendingSelection;

    std::vector<ServerListOption> listOptions;
    int listSelectedIndex = -1;
    std::optional<int> pendingListSelection;
    std::optional<ServerListOption> pendingNewList;
    bool refreshRequested = false;
    bool scanning = false;
    std::array<char, 512> listUrlBuffer{};
    std::string listStatusText;
    bool listStatusIsError = false;
    std::string communityStatusText;
    MessageTone communityStatusTone = MessageTone::Notice;
    int lastCredentialsListIndex = -1;
    std::string storedPasswordHash;

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

} // namespace gui
