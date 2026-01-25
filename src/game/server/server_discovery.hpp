#pragma once

#include <cstdint>
#include <string>
#include <thread>
#include <atomic>

class ServerDiscoveryBeacon {
public:
    ServerDiscoveryBeacon(uint16_t listenPort, std::string serverName, std::string worldName);
    ~ServerDiscoveryBeacon();

    ServerDiscoveryBeacon(const ServerDiscoveryBeacon&) = delete;
    ServerDiscoveryBeacon& operator=(const ServerDiscoveryBeacon&) = delete;

private:
    void run();
    void closeSocket();

    int socketFd = -1;
    std::thread worker;
    std::atomic<bool> running{false};
    uint16_t listenPort;
    std::string serverName;
    std::string worldName;
    uint32_t instanceId = 0;
};
