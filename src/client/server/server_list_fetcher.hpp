#pragma once

#include "client/config_client.hpp"
#include <vector>
#include <string>
#include <mutex>
#include <thread>
#include <atomic>
#include <cstdint>

class ServerListFetcher {
public:
    struct SourceStatus {
        std::string sourceHost;
        std::string communityName;
        std::string communityDetails;
        int activeCount = -1;
        int inactiveCount = -1;
        bool ok = false;
        bool hasData = false;
        std::string error;
    };

    struct ServerRecord {
        std::string sourceName;
        std::string sourceHost;
        std::string name;
        std::string code;
        std::string host;
        uint16_t port = 0;
        int maxPlayers = -1;
        int activePlayers = -1;
        std::string gameMode;
        std::string overview;
        std::string detailDescription;
        std::vector<std::string> flags;
        std::string screenshotId;
        std::string communityName;
        int activeCount = -1;
        int inactiveCount = -1;
    };

    explicit ServerListFetcher(std::vector<ClientServerListSource> sources);
    ~ServerListFetcher();

    void requestRefresh();
    std::vector<ServerRecord> getServers() const;
    std::vector<SourceStatus> getSourceStatuses() const;
    std::size_t getGeneration() const;
    bool isFetching() const;

private:
    void launchWorker();
    void workerProc();
    std::vector<ServerRecord> fetchOnce();
    static bool fetchUrl(const std::string &url, std::string &outBody);
    static std::vector<ServerRecord> parseResponse(const ClientServerListSource &source,
                                                   const std::string &body,
                                                   SourceStatus *statusOut);

    std::vector<ClientServerListSource> sources;
    mutable std::mutex recordsMutex;
    std::vector<ServerRecord> records;
    std::vector<SourceStatus> sourceStatuses;
    std::atomic<bool> fetching{false};
    std::atomic<std::size_t> generation{0};
    std::thread worker;
    bool curlInitialized = false;
};
