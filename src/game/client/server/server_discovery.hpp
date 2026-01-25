#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <unordered_map>
#include <chrono>

class ServerDiscovery {
public:
    struct DiscoveredServer {
        std::string host;
        uint16_t port;
        std::string name;
        std::string world;
        std::string displayHost;
        uint32_t serverId = 0;
        bool hasPublicAddress = false;
    };

    ServerDiscovery();
    ~ServerDiscovery();

    void startScan();
    void update();
    bool isScanning() const;
    const std::vector<DiscoveredServer>& getServers() const;
    std::size_t getGeneration() const;
    void clearServers();

private:
    void closeSocket();
    void sendDiscoveryRequest(uint32_t address);

    int socketFd = -1;
    bool scanning = false;
    std::chrono::steady_clock::time_point scanDeadline;

    std::vector<DiscoveredServer> servers;
    std::unordered_map<uint32_t, std::size_t> serverIndexById;
    std::unordered_map<std::string, std::size_t> serverIndexByAddress;
    std::size_t generation = 0;
};
