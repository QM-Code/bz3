#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "client/config_client.hpp"
#include "client/server/server_discovery.hpp"
#include "client/server/server_list_fetcher.hpp"
#include "engine/client_engine.hpp"
#include "engine/components/gui/server_browser.hpp"

class ServerConnector;

class ServerBrowserController {
public:
    ServerBrowserController(ClientEngine &engine,
                            ClientConfig &clientConfig,
                            const std::string &configPath,
                            const std::string &defaultHost,
                            uint16_t defaultPort,
                            ServerConnector &connector);

    void update();
    void handleDisconnected(const std::string &reason);

private:
    using SteadyClock = std::chrono::steady_clock;

    void triggerFullRefresh();
    void rebuildEntries();
    void refreshGuiServerListOptions();
    void rebuildServerListFetcher();
    std::vector<ClientServerListSource> resolveActiveServerLists() const;
    void handleServerListSelection(int selectedIndex);
    void handleServerListAddition(const gui::ServerListOption &option);
    void updateServerListDisplayNamesFromCache();
    std::string resolveDisplayNameForSource(const ClientServerListSource &source) const;
    int getLanOffset() const;
    int totalListOptionCount() const;
    bool isLanIndex(int index) const;
    bool isLanSelected() const;
    const ClientServerListSource* getSelectedRemoteSource() const;
    int computeDefaultSelectionIndex(int optionCount) const;

    ClientEngine &engine;
    gui::ServerBrowserView &browser;
    ClientConfig &clientConfig;
    std::string clientConfigPath;
    ServerConnector &connector;
    ServerDiscovery discovery;
    std::unique_ptr<ServerListFetcher> serverListFetcher;
    std::vector<ServerListFetcher::ServerRecord> cachedRemoteServers;
    std::vector<gui::ServerBrowserEntry> lastGuiEntries;
    std::string defaultHost;
    uint16_t defaultPort = 0;
    int activeServerListIndex = -1;
    std::unordered_map<std::string, std::string> serverListDisplayNames;
    std::chrono::seconds communityAutoRefreshInterval{0};
    std::chrono::seconds lanAutoRefreshInterval{0};

    SteadyClock::time_point nextRemoteRefreshTime{};

    SteadyClock::time_point nextAutoScanTime;
    std::size_t lastDiscoveryVersion = 0;
    std::size_t lastServerListGeneration = 0;
};
