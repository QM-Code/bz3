#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "client/config_client.hpp"
#include "client/server/community_auth_client.hpp"
#include "client/server/server_discovery.hpp"
#include "client/server/server_list_fetcher.hpp"
#include "engine/client_engine.hpp"
#include "ui/console/console_interface.hpp"

class ServerConnector;

class CommunityBrowserController {
public:
    CommunityBrowserController(ClientEngine &engine,
                            ClientConfig &clientConfig,
                            const std::string &configPath,
                            ServerConnector &connector);
    ~CommunityBrowserController();

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
    void handleServerListAddition(const ui::ServerListOption &option);
    void commitServerListAddition(const std::string &baseUrl);
    void handleServerListDeletion(const std::string &host);
    void updateServerListDisplayNamesFromCache();
    void updateCommunityDetails();
    std::string makeServerDetailsKey(const ui::CommunityBrowserEntry &entry) const;
    std::string makeServerDetailsKey(const ServerListFetcher::ServerRecord &record) const;
    void startServerDetailsRequest(const ui::CommunityBrowserEntry &entry);
    std::string resolveDisplayNameForSource(const ClientServerListSource &source) const;
    int getLanOffset() const;
    int totalListOptionCount() const;
    bool isLanIndex(int index) const;
    bool isLanSelected() const;
    const ClientServerListSource* getSelectedRemoteSource() const;
    int computeDefaultSelectionIndex(int optionCount) const;
    void handleJoinSelection(const ui::CommunityBrowserSelection &selection);
    void handleAuthResponse(const CommunityAuthClient::Response &response);
    std::string resolveCommunityHost(const ui::CommunityBrowserSelection &selection) const;
    std::string makeAuthCacheKey(const std::string &host, const std::string &username) const;

    ClientEngine &engine;
    ui::ConsoleInterface &browser;
    ClientConfig &clientConfig;
    std::string clientConfigPath;
    ServerConnector &connector;
    ServerDiscovery discovery;
    std::unique_ptr<ServerListFetcher> serverListFetcher;
    std::vector<ServerListFetcher::ServerRecord> cachedRemoteServers;
    std::vector<ServerListFetcher::SourceStatus> cachedSourceStatuses;
    std::vector<ui::CommunityBrowserEntry> lastGuiEntries;
    int activeServerListIndex = -1;
    std::unordered_map<std::string, std::string> serverListDisplayNames;
    std::unordered_map<std::string, std::string> serverDescriptionCache;
    std::unordered_set<std::string> serverDescriptionFailed;
    std::unordered_map<std::string, std::string> serverDescriptionErrors;
    std::string selectedServerKey;
    struct ServerDetailsRequest {
        std::string key;
        std::string sourceHost;
        std::string serverName;
        std::string detailName;
        std::string description;
        std::atomic<bool> done{false};
        bool ok = false;
        std::thread worker;
    };
    std::unique_ptr<ServerDetailsRequest> serverDetailsRequest;
    struct PendingAddRequest {
        std::string baseUrl;
        std::string displayHost;
        std::atomic<bool> done{false};
        bool ok = false;
        std::thread worker;
    };
    std::unique_ptr<PendingAddRequest> pendingAddRequest;
    bool curlReady = false;
    CommunityAuthClient authClient;
    std::unordered_map<std::string, std::string> passwordSaltCache;
    struct PendingJoin {
        ui::CommunityBrowserSelection selection;
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
