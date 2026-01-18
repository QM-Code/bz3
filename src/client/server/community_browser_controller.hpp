#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "client/config_client.hpp"
#include "client/server/community_auth_client.hpp"
#include "client/server/server_discovery.hpp"
#include "client/server/server_list_fetcher.hpp"
#include "engine/client_engine.hpp"
#include "engine/components/gui/main_menu.hpp"

class ServerConnector;

class CommunityBrowserController {
public:
    CommunityBrowserController(ClientEngine &engine,
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
    void handleJoinSelection(const gui::CommunityBrowserSelection &selection);
    void handleAuthResponse(const CommunityAuthClient::Response &response);
    std::string resolveCommunityHost(const gui::CommunityBrowserSelection &selection) const;
    std::string makeAuthCacheKey(const std::string &host, const std::string &username) const;

    ClientEngine &engine;
    gui::MainMenuView &browser;
    ClientConfig &clientConfig;
    std::string clientConfigPath;
    ServerConnector &connector;
    ServerDiscovery discovery;
    std::unique_ptr<ServerListFetcher> serverListFetcher;
    std::vector<ServerListFetcher::ServerRecord> cachedRemoteServers;
    std::vector<ServerListFetcher::SourceStatus> cachedSourceStatuses;
    std::vector<gui::CommunityBrowserEntry> lastGuiEntries;
    std::string defaultHost;
    uint16_t defaultPort = 0;
    int activeServerListIndex = -1;
    std::unordered_map<std::string, std::string> serverListDisplayNames;
    CommunityAuthClient authClient;
    std::unordered_map<std::string, std::string> passwordSaltCache;
    struct PendingJoin {
        gui::CommunityBrowserSelection selection;
        std::string communityHost;
        std::string username;
        std::string password;
        bool communityAdmin = false;
        bool localAdmin = false;
        bool awaitingAuth = false;
    };
    std::optional<PendingJoin> pendingJoin;

    std::size_t lastDiscoveryVersion = 0;
    std::size_t lastServerListGeneration = 0;
};
