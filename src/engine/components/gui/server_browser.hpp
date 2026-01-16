#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include <unordered_map>

struct ImGuiIO;
struct ImFont;

namespace gui {

struct ServerBrowserEntry {
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
};

struct ThumbnailTexture {
    unsigned int textureId = 0;
    int width = 0;
    int height = 0;
    bool failed = false;
};

struct ServerBrowserSelection {
    std::string host;
    uint16_t port;
    bool fromPreset;
};

struct ServerListOption {
    std::string name;
    std::string host;
};

class ServerBrowserView {
public:
    ~ServerBrowserView();
    void initializeFonts(ImGuiIO &io);
    void draw(ImGuiIO &io);

    void show(const std::vector<ServerBrowserEntry> &entries,
              const std::string &defaultHost,
              uint16_t defaultPort);
    void setEntries(const std::vector<ServerBrowserEntry> &entries);
    void setListOptions(const std::vector<ServerListOption> &options, int selectedIndex);
    void hide();
    bool isVisible() const;
    void setStatus(const std::string &statusText, bool isErrorMessage);
    std::optional<ServerBrowserSelection> consumeSelection();
    std::optional<int> consumeListSelection();
    std::optional<ServerListOption> consumeNewListRequest();
    void setListStatus(const std::string &statusText, bool isErrorMessage);
    void clearNewListInputs();
    bool consumeRefreshRequest();
    void setScanning(bool scanning);

private:
    void resetBuffers(const std::string &defaultHost, uint16_t defaultPort);
    ThumbnailTexture *getOrLoadThumbnail(const std::string &url);
    void clearThumbnails();

    bool visible = false;
    ImFont *regularFont = nullptr;
    ImFont *headingFont = nullptr;
    ImFont *buttonFont = nullptr;

    std::vector<ServerBrowserEntry> entries;
    int selectedIndex = -1;
    std::array<char, 256> addressBuffer{};
    std::string statusText;
    bool statusIsError = false;
    std::optional<ServerBrowserSelection> pendingSelection;

    std::vector<ServerListOption> listOptions;
    int listSelectedIndex = -1;
    std::optional<int> pendingListSelection;
    std::optional<ServerListOption> pendingNewList;
    bool refreshRequested = false;
    bool scanning = false;
    std::array<char, 512> listUrlBuffer{};
    std::string listStatusText;
    bool listStatusIsError = false;

    std::unordered_map<std::string, ThumbnailTexture> thumbnailCache;
};

} // namespace gui
