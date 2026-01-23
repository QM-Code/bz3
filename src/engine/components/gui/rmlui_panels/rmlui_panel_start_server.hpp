#pragma once

#include <atomic>
#include <deque>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace Rml {
class Element;
class ElementDocument;
class EventListener;
}

#include "engine/components/gui/main_menu_types.hpp"
#include "engine/components/gui/rmlui_modal_dialog.hpp"
#include "engine/components/gui/rmlui_panels/rmlui_panel.hpp"

namespace gui {

class RmlUiPanelStartServer final : public RmlUiPanel {
public:
    RmlUiPanelStartServer();
    ~RmlUiPanelStartServer() override;

    void setListOptions(const std::vector<ServerListOption> &options, int selectedIndex);

protected:
    void onLoaded(Rml::ElementDocument *document) override;
    void onUpdate() override;

private:
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

    class StartServerListener;
    class ServerRowListener;
    class ServerActionListener;
    class ServerLogLevelListener;

    void handleRefreshIp();
    void handleAdvertiseChanged();
    void handleStartServer();
    void handlePortChanged();
    void handlePortIncrement(int delta);
    void handleCommunityChanged();
    void handleWorldChanged();
    void handleWorldPickChanged();
    void handleLogLevelChanged();
    void handleSelectServer(int serverId);
    void handleServerAction(int serverId, const std::string &action);
    void handleServerLogLevel(int serverId, int logIndex);

    void updateCommunitySelect();
    void updateWorldSelect();
    void updateServerList();
    void updateLogOutput();
    void updateStatusText();
    void showPortError(const std::string &message);
    bool ensureAdvertiseHost();

    void stopLocalServer(std::size_t index);
    void stopAllLocalServers();
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
    std::string findServerBinary();
    std::optional<std::size_t> findServerIndex(int serverId) const;

    Rml::ElementDocument *document = nullptr;
    Rml::Element *panelRoot = nullptr;
    Rml::Element *warningText = nullptr;
    Rml::Element *statusText = nullptr;
    Rml::Element *advertiseInput = nullptr;
    Rml::Element *portInput = nullptr;
    Rml::Element *communitySelect = nullptr;
    Rml::Element *communityEmptyText = nullptr;
    Rml::Element *worldInput = nullptr;
    Rml::Element *worldSelect = nullptr;
    Rml::Element *logLevelSelect = nullptr;
    Rml::Element *startButton = nullptr;
    Rml::Element *runningList = nullptr;
    Rml::Element *logOutput = nullptr;
    RmlUiModalDialog errorDialog;

    std::vector<ServerListOption> listOptions;
    int listSelectedIndex = -1;
    int serverCommunityIndex = -1;
    int serverLogLevelIndex = 2;
    int serverPortValue = 11899;
    int nextLocalServerId = 1;
    int selectedLogServerId = -1;
    bool serverBinaryChecked = false;
    std::string serverBinaryPath;
    std::string serverStatusText;
    bool serverStatusIsError = false;
    std::string advertiseHostValue;
    std::string worldPathValue;
    std::string lastLogSnapshot;
    std::size_t lastListSignature = 0;
    std::vector<std::string> worldChoices;
    std::vector<std::unique_ptr<LocalServerProcess>> localServers;
    std::vector<std::unique_ptr<Rml::EventListener>> listeners;
    std::vector<std::unique_ptr<Rml::EventListener>> dynamicListeners;
};

} // namespace gui
