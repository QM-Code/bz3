#include "client/server/server_discovery.hpp"
#include "game/net/discovery_protocol.hpp"
#include "spdlog/spdlog.h"

#include <cstring>
#include <cerrno>

#if defined(_WIN32)
    #include <winsock2.h>
    #include <ws2tcpip.h>
#else
    #include <arpa/inet.h>
    #include <netinet/in.h>
    #include <sys/socket.h>
    #include <fcntl.h>
    #include <unistd.h>
#endif

namespace {
std::string sanitizeLabel(const char* buffer, std::size_t size) {
    std::size_t length = 0;
    while (length < size && buffer[length] != '\0') {
        ++length;
    }
    return std::string(buffer, length);
}

#if defined(_WIN32)
void setNonBlocking(SOCKET fd) {
    u_long mode = 1;
    ioctlsocket(fd, FIONBIO, &mode);
}
#else
void setNonBlocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        flags = 0;
    }
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}
#endif

#if defined(_WIN32)
void closeSocketHandle(SOCKET fd) {
    if (fd != INVALID_SOCKET) {
        closesocket(fd);
    }
}
#else
void closeSocketHandle(int fd) {
    if (fd >= 0) {
        close(fd);
    }
}
#endif
}

constexpr const char* LOOPBACK_DISPLAY = "localhost / 127.0.0.1";

bool isLoopbackAddress(const in_addr &addr) {
    return ntohl(addr.s_addr) == INADDR_LOOPBACK;
}

std::string makeAddressKey(const std::string &host, uint16_t port) {
    return host + ":" + std::to_string(port);
}

ServerDiscovery::ServerDiscovery() {
#if defined(_WIN32)
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        spdlog::warn("ServerDiscovery: Failed to initialize Winsock.");
    }
#endif
}

ServerDiscovery::~ServerDiscovery() {
    closeSocket();
#if defined(_WIN32)
    WSACleanup();
#endif
}

void ServerDiscovery::startScan() {
    clearServers();
    closeSocket();

    socketFd = static_cast<int>(socket(AF_INET, SOCK_DGRAM, 0));
    if (socketFd < 0) {
        spdlog::warn("ServerDiscovery: Unable to create socket for LAN scan.");
        return;
    }

    int reuse = 1;
    if (setsockopt(socketFd, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<char*>(&reuse), sizeof(reuse)) < 0) {
        spdlog::warn("ServerDiscovery: Failed to set SO_REUSEADDR.");
    }

    int broadcast = 1;
    if (setsockopt(socketFd, SOL_SOCKET, SO_BROADCAST, reinterpret_cast<char*>(&broadcast), sizeof(broadcast)) < 0) {
        spdlog::warn("ServerDiscovery: Failed to enable broadcast.");
    }

    sockaddr_in local{};
    local.sin_family = AF_INET;
    local.sin_port = 0;
    local.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(socketFd, reinterpret_cast<sockaddr*>(&local), sizeof(local)) < 0) {
        spdlog::warn("ServerDiscovery: Failed to bind discovery socket.");
        closeSocket();
        return;
    }

    setNonBlocking(socketFd);

    // Broadcast to the LAN
    sendDiscoveryRequest(INADDR_BROADCAST);
    // Directly ping loopback to detect localhost server without listing unless it responds
    sendDiscoveryRequest(INADDR_LOOPBACK);

    scanning = true;
    scanDeadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(1500);
}

void ServerDiscovery::update() {
    if (!scanning || socketFd < 0) {
        return;
    }

    while (true) {
        DiscoveryProtocol::ResponsePacket response{};
        sockaddr_in from{};
        socklen_t fromLen = sizeof(from);
        int received = static_cast<int>(recvfrom(socketFd, reinterpret_cast<char*>(&response), sizeof(response), 0,
            reinterpret_cast<sockaddr*>(&from), &fromLen));

        if (received < 0) {
#if defined(_WIN32)
            int err = WSAGetLastError();
            if (err == WSAEWOULDBLOCK) {
                break;
            }
#else
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                break;
            }
#endif
            spdlog::warn("ServerDiscovery: recvfrom failed while scanning.");
            break;
        }

        if (received < static_cast<int>(sizeof(DiscoveryProtocol::PacketHeader))) {
            continue;
        }

        if (!DiscoveryProtocol::isResponse(response)) {
            continue;
        }

        char ipBuffer[INET_ADDRSTRLEN] = {0};
        inet_ntop(AF_INET, &from.sin_addr, ipBuffer, sizeof(ipBuffer));
        std::string host(ipBuffer);

        uint16_t port = ntohs(response.gamePort);
        uint32_t serverId = response.serverId;
        bool loopback = isLoopbackAddress(from.sin_addr);

        std::string name = sanitizeLabel(response.serverName, sizeof(response.serverName));
        std::string world = sanitizeLabel(response.worldName, sizeof(response.worldName));

        auto updateEntry = [&](std::size_t index) {
            auto &entry = servers[index];
            bool changed = false;
            if (!loopback) {
                if (entry.host != host) {
                    entry.host = host;
                    changed = true;
                }
                if (entry.displayHost != host) {
                    entry.displayHost = host;
                    changed = true;
                }
                if (!entry.hasPublicAddress) {
                    entry.hasPublicAddress = true;
                    changed = true;
                }
            } else if (!entry.hasPublicAddress) {
                if (entry.displayHost != LOOPBACK_DISPLAY) {
                    entry.displayHost = LOOPBACK_DISPLAY;
                    changed = true;
                }
                if (entry.host != host) {
                    entry.host = host;
                    changed = true;
                }
            }

            if (entry.name != name) {
                entry.name = name;
                changed = true;
            }

            if (entry.world != world) {
                entry.world = world;
                changed = true;
            }

            if (changed) {
                ++generation;
            }
        };

        bool handled = false;

        if (serverId != 0) {
            auto it = serverIndexById.find(serverId);
            if (it != serverIndexById.end()) {
                updateEntry(it->second);
                handled = true;
            }
        }

        if (!handled) {
            std::string key = makeAddressKey(host, port);
            auto itAddr = serverIndexByAddress.find(key);
            if (itAddr != serverIndexByAddress.end()) {
                updateEntry(itAddr->second);
                handled = true;
            }
        }

        if (handled) {
            continue;
        }

        DiscoveredServer entry;
        entry.host = host;
        entry.port = port;
        entry.name = name;
        entry.world = world;
        entry.displayHost = loopback ? LOOPBACK_DISPLAY : host;
        entry.serverId = serverId;
        entry.hasPublicAddress = !loopback;

        servers.push_back(entry);
        std::size_t newIndex = servers.size() - 1;

        if (serverId != 0) {
            serverIndexById[serverId] = newIndex;
        } else {
            serverIndexByAddress[makeAddressKey(host, port)] = newIndex;
        }

        ++generation;
        spdlog::info("Discovered LAN server {}:{} ({})", host, port, name);
    }

    if (std::chrono::steady_clock::now() >= scanDeadline) {
        scanning = false;
        closeSocket();
    }
}

bool ServerDiscovery::isScanning() const {
    return scanning;
}

const std::vector<ServerDiscovery::DiscoveredServer>& ServerDiscovery::getServers() const {
    return servers;
}

std::size_t ServerDiscovery::getGeneration() const {
    return generation;
}

void ServerDiscovery::clearServers() {
    if (servers.empty() && serverIndexById.empty() && serverIndexByAddress.empty()) {
        return;
    }

    servers.clear();
    serverIndexById.clear();
    serverIndexByAddress.clear();
    ++generation;
}

void ServerDiscovery::closeSocket() {
    if (socketFd >= 0) {
        closeSocketHandle(socketFd);
        socketFd = -1;
    }
    scanning = false;
}

void ServerDiscovery::sendDiscoveryRequest(uint32_t address) {
    if (socketFd < 0) {
        return;
    }

    DiscoveryProtocol::RequestPacket request{};

    sockaddr_in target{};
    target.sin_family = AF_INET;
    target.sin_port = htons(DiscoveryProtocol::PORT);
    target.sin_addr.s_addr = htonl(address);

    sendto(socketFd, reinterpret_cast<const char*>(&request), sizeof(request), 0,
        reinterpret_cast<sockaddr*>(&target), sizeof(target));
}
