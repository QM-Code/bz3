#pragma once

#include <array>
#include <cstdint>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>
#include <unordered_map>
#include <unordered_set>

struct ImGuiIO;
struct ImFont;

#include <imgui.h>

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
    std::string worldName;
};

struct ThumbnailTexture {
    unsigned int textureId = 0;
    int width = 0;
    int height = 0;
    bool failed = false;
    bool loading = false;
};

struct ServerBrowserSelection {
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

class ServerBrowserView {
public:
    enum class MessageTone {
        Notice,
        Error,
        Pending
    };

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
    void setCustomStatus(const std::string &statusText, bool isErrorMessage);
    std::optional<ServerBrowserSelection> consumeSelection();
    std::optional<int> consumeListSelection();
    std::optional<ServerListOption> consumeNewListRequest();
    void setListStatus(const std::string &statusText, bool isErrorMessage);
    void clearNewListInputs();
    std::string getUsername() const;
    std::string getPassword() const;
    void clearPassword();
    void setCommunityStatus(const std::string &text, MessageTone tone);
    bool consumeRefreshRequest();
    void setScanning(bool scanning);

private:
    struct MessageColors {
        ImVec4 error;
        ImVec4 notice;
        ImVec4 action;
        ImVec4 pending;
    };

    struct ThumbnailPayload {
        std::string url;
        int width = 0;
        int height = 0;
        bool failed = true;
        std::vector<unsigned char> pixels;
    };

    void resetBuffers(const std::string &defaultHost, uint16_t defaultPort);
    ThumbnailTexture *getOrLoadThumbnail(const std::string &url);
    void clearThumbnails();
    void startThumbnailWorker();
    void stopThumbnailWorker();
    void queueThumbnailRequest(const std::string &url);
    void processThumbnailUploads();
    void thumbnailWorkerProc();
    MessageColors getMessageColors() const;

    bool visible = false;
    ImFont *regularFont = nullptr;
    ImFont *headingFont = nullptr;
    ImFont *buttonFont = nullptr;

    std::vector<ServerBrowserEntry> entries;
    int selectedIndex = -1;
    std::array<char, 256> addressBuffer{};
    std::array<char, 64> usernameBuffer{};
    std::array<char, 128> passwordBuffer{};
    std::string statusText;
    bool statusIsError = false;
    std::string customStatusText;
    bool customStatusIsError = false;
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
    std::string communityStatusText;
    MessageTone communityStatusTone = MessageTone::Notice;

    std::unordered_map<std::string, ThumbnailTexture> thumbnailCache;
    std::deque<std::string> thumbnailRequests;
    std::deque<ThumbnailPayload> thumbnailResults;
    std::unordered_set<std::string> thumbnailInFlight;
    std::mutex thumbnailMutex;
    std::condition_variable thumbnailCv;
    std::thread thumbnailWorker;
    bool thumbnailWorkerStop = false;
};

} // namespace gui
