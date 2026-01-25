#include "server/server_discovery.hpp"
#include "game/net/discovery_protocol.hpp"
#include "spdlog/spdlog.h"

#include <cstring>
#include <cstdio>
#include <random>
#include <limits>

#if defined(_WIN32)
    #include <winsock2.h>
    #include <ws2tcpip.h>
#else
    #include <arpa/inet.h>
    #include <netinet/in.h>
    #include <sys/socket.h>
    #include <sys/select.h>
    #include <unistd.h>
    #include <errno.h>
#endif

namespace {
#if defined(_WIN32)
void CloseSocketHandle(SOCKET fd) {
    if (fd != INVALID_SOCKET) {
        closesocket(fd);
    }
}
#else
void CloseSocketHandle(int fd) {
    if (fd >= 0) {
        close(fd);
    }
}
#endif
}

ServerDiscoveryBeacon::ServerDiscoveryBeacon(uint16_t listenPort, std::string serverName, std::string worldName)
    : listenPort(listenPort),
      serverName(std::move(serverName)),
      worldName(std::move(worldName)) {
#if defined(_WIN32)
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        spdlog::warn("ServerDiscoveryBeacon: Failed to initialize Winsock.");
        return;
    }
#endif

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint32_t> dist(1, std::numeric_limits<uint32_t>::max());
    instanceId = dist(gen);

    socketFd = static_cast<int>(socket(AF_INET, SOCK_DGRAM, 0));
    if (socketFd < 0) {
        spdlog::warn("ServerDiscoveryBeacon: Failed to create socket.");
        return;
    }

    int reuse = 1;
    if (setsockopt(socketFd, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<char*>(&reuse), sizeof(reuse)) < 0) {
        spdlog::warn("ServerDiscoveryBeacon: Failed to set SO_REUSEADDR.");
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(DiscoveryProtocol::PORT);

    if (bind(socketFd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        spdlog::warn("ServerDiscoveryBeacon: Failed to bind discovery socket on port {}.", DiscoveryProtocol::PORT);
        CloseSocketHandle(socketFd);
        socketFd = -1;
        return;
    }

    running = true;
    worker = std::thread(&ServerDiscoveryBeacon::run, this);
    spdlog::info("ServerDiscoveryBeacon listening for LAN discovery on UDP {}", DiscoveryProtocol::PORT);
}

ServerDiscoveryBeacon::~ServerDiscoveryBeacon() {
    running = false;
    closeSocket();
    if (worker.joinable()) {
        worker.join();
    }
#if defined(_WIN32)
    WSACleanup();
#endif
}

void ServerDiscoveryBeacon::closeSocket() {
    if (socketFd >= 0) {
#if defined(_WIN32)
        closesocket(static_cast<SOCKET>(socketFd));
#else
        close(socketFd);
#endif
        socketFd = -1;
    }
}

void ServerDiscoveryBeacon::run() {
    while (running && socketFd >= 0) {
        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(socketFd, &readSet);

        timeval timeout{};
        timeout.tv_sec = 0;
        timeout.tv_usec = 250000; // 250 ms

        int ready = select(socketFd + 1, &readSet, nullptr, nullptr, &timeout);
        if (ready < 0) {
#if !defined(_WIN32)
            if (errno == EINTR) {
                continue;
            }
#endif
            spdlog::warn("ServerDiscoveryBeacon: select() failed.");
            break;
        }

        if (ready == 0) {
            continue;
        }

        sockaddr_in from{};
        socklen_t fromLen = sizeof(from);
        DiscoveryProtocol::RequestPacket request{};
        int received = static_cast<int>(recvfrom(socketFd, reinterpret_cast<char*>(&request), sizeof(request), 0,
            reinterpret_cast<sockaddr*>(&from), &fromLen));

        if (received < static_cast<int>(sizeof(DiscoveryProtocol::PacketHeader))) {
            continue;
        }

        if (!DiscoveryProtocol::isRequest(request)) {
            continue;
        }

        DiscoveryProtocol::ResponsePacket response{};
        response.gamePort = htons(listenPort);
        response.serverId = instanceId;
        std::snprintf(response.serverName, sizeof(response.serverName), "%s", serverName.c_str());
        std::snprintf(response.worldName, sizeof(response.worldName), "%s", worldName.c_str());

        sendto(socketFd, reinterpret_cast<const char*>(&response), sizeof(response), 0,
            reinterpret_cast<sockaddr*>(&from), fromLen);
    }
}
