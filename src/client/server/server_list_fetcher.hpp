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
    struct ServerRecord {
        std::string sourceName;
        std::string sourceUrl;
        std::string name;
        std::string host;
        uint16_t port = 0;
        int maxPlayers = -1;
        int activePlayers = -1;
        std::string gameMode;
        std::string description;
        std::vector<std::string> flags;
    };

    explicit ServerListFetcher(std::vector<ClientServerListSource> sources);
    ~ServerListFetcher();

    void requestRefresh();
    std::vector<ServerRecord> getServers() const;
    std::size_t getGeneration() const;
    bool isFetching() const;

private:
    void launchWorker();
    void workerProc();
    std::vector<ServerRecord> fetchOnce() const;
    static bool fetchUrl(const std::string &url, std::string &outBody);
    static std::vector<ServerRecord> parseResponse(const ClientServerListSource &source, const std::string &body);

    std::vector<ClientServerListSource> sources;
    mutable std::mutex recordsMutex;
    std::vector<ServerRecord> records;
    std::atomic<bool> fetching{false};
    std::atomic<std::size_t> generation{0};
    std::thread worker;
    bool curlInitialized = false;
};
