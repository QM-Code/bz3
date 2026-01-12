#include "network/enet_transport.hpp"

#include <enet.h>
#include <spdlog/spdlog.h>

#include <array>
#include <atomic>
#include <cstring>
#include <mutex>

namespace net {
namespace {

class EnetGlobal {
public:
    EnetGlobal() {
        std::lock_guard<std::mutex> lock(mutex());
        if (refCount()++ == 0) {
            if (enet_initialize() != 0) {
                spdlog::error("ENet: failed to initialize");
            }
        }
    }

    ~EnetGlobal() {
        std::lock_guard<std::mutex> lock(mutex());
        auto &count = refCount();
        if (count == 0) {
            return;
        }
        if (--count == 0) {
            enet_deinitialize();
        }
    }

private:
    static std::mutex &mutex() {
        static std::mutex m;
        return m;
    }

    static std::uint32_t &refCount() {
        static std::uint32_t count = 0;
        return count;
    }
};

ENetPacketFlag toEnetFlag(Delivery delivery) {
    switch (delivery) {
    case Delivery::Reliable:
        return ENET_PACKET_FLAG_RELIABLE;
    case Delivery::Unreliable:
        return static_cast<ENetPacketFlag>(0);
    default:
        return ENET_PACKET_FLAG_RELIABLE;
    }
}

enet_uint8 toEnetChannel(Delivery delivery, int numChannels) {
    if (delivery == Delivery::Unreliable && numChannels > 1) {
        return 1;
    }
    return 0;
}

std::optional<std::string> peerIpString(const ENetAddress &addr) {
    std::array<char, 128> ipBuffer{};
    if (enet_address_get_host_ip(&addr, ipBuffer.data(), ipBuffer.size()) == 0) {
        return std::string(ipBuffer.data());
    }
    return std::nullopt;
}

class EnetClientTransport final : public IClientTransport {
public:
    EnetClientTransport() = default;
    ~EnetClientTransport() override {
        disconnect();
        if (host) {
            enet_host_destroy(host);
            host = nullptr;
        }
    }

    bool connect(const std::string &hostName, uint16_t port, int timeoutMs) override {
        disconnect();
        remoteIp.reset();
        remotePort.reset();

        if (!host) {
            host = enet_host_create(nullptr, 1, channelCount, 0, 0);
            if (!host) {
                spdlog::error("ENet client: failed to create host");
                return false;
            }
        }

        ENetAddress address;
        if (enet_address_set_host(&address, hostName.c_str()) != 0) {
            spdlog::error("ENet client: failed to resolve host {}", hostName);
            return false;
        }
        address.port = port;

        peer = enet_host_connect(host, &address, 2, 0);
        if (!peer) {
            spdlog::error("ENet client: no available peers");
            return false;
        }

        ENetEvent event;
        if (enet_host_service(host, &event, timeoutMs) > 0 && event.type == ENET_EVENT_TYPE_CONNECT) {
            remoteIp = peerIpString(event.peer->address);
            remotePort = event.peer->address.port;
            enet_host_flush(host);
            return true;
        }

        enet_peer_reset(peer);
        peer = nullptr;
        return false;
    }

    void disconnect() override {
        if (peer) {
            enet_peer_disconnect(peer, 0);
            peer = nullptr;
        }
    }

    bool isConnected() const override {
        return peer != nullptr;
    }

    void poll(std::vector<Event> &outEvents) override {
        if (!host) {
            return;
        }

        ENetEvent event;
        while (enet_host_service(host, &event, 0) > 0) {
            switch (event.type) {
            case ENET_EVENT_TYPE_RECEIVE: {
                Event e;
                e.type = Event::Type::Receive;
                e.connection = reinterpret_cast<ConnectionHandle>(event.peer);
                e.payload.resize(event.packet->dataLength);
                std::memcpy(e.payload.data(), event.packet->data, event.packet->dataLength);
                outEvents.push_back(std::move(e));
                enet_packet_destroy(event.packet);
                break;
            }
            case ENET_EVENT_TYPE_DISCONNECT: {
                Event e;
                e.type = Event::Type::Disconnect;
                e.connection = reinterpret_cast<ConnectionHandle>(event.peer);
                if (auto ip = peerIpString(event.peer->address)) {
                    e.peerIp = *ip;
                }
                e.peerPort = event.peer->address.port;
                outEvents.push_back(std::move(e));
                peer = nullptr;
                remoteIp.reset();
                remotePort.reset();
                break;
            }
            case ENET_EVENT_TYPE_DISCONNECT_TIMEOUT: {
                Event e;
                e.type = Event::Type::DisconnectTimeout;
                e.connection = reinterpret_cast<ConnectionHandle>(event.peer);
                if (auto ip = peerIpString(event.peer->address)) {
                    e.peerIp = *ip;
                }
                e.peerPort = event.peer->address.port;
                outEvents.push_back(std::move(e));
                peer = nullptr;
                remoteIp.reset();
                remotePort.reset();
                break;
            }
            default:
                break;
            }
        }
    }

    void send(const std::byte *data, std::size_t size, Delivery delivery, bool flush) override {
        if (!host || !peer) {
            return;
        }

        ENetPacket *packet = enet_packet_create(data, size, toEnetFlag(delivery));
        enet_peer_send(peer, toEnetChannel(delivery, 2), packet);

        if (flush) {
            enet_host_flush(host);
        }
    }

    std::optional<std::string> getRemoteIp() const override {
        return remoteIp;
    }

    std::optional<uint16_t> getRemotePort() const override {
        return remotePort;
    }

private:
    EnetGlobal global;
    ENetHost *host = nullptr;
    ENetPeer *peer = nullptr;
    std::optional<std::string> remoteIp;
    std::optional<uint16_t> remotePort;
    static constexpr int channelCount = 2;
};

class EnetServerTransport final : public IServerTransport {
public:
    EnetServerTransport(uint16_t port, int maxClients, int numChannels) {
        ENetAddress address;
        address.host = ENET_HOST_ANY;
        address.port = port;

        host = enet_host_create(&address, maxClients, numChannels, 0, 0);
        channelCount = numChannels;
        if (!host) {
            spdlog::error("ENet server: failed to create host on port {}", port);
        }
    }

    ~EnetServerTransport() override {
        if (host) {
            enet_host_destroy(host);
            host = nullptr;
        }
    }

    void poll(std::vector<Event> &outEvents) override {
        if (!host) {
            return;
        }

        ENetEvent event;
        while (enet_host_service(host, &event, 0) > 0) {
            switch (event.type) {
            case ENET_EVENT_TYPE_CONNECT: {
                Event e;
                e.type = Event::Type::Connect;
                e.connection = reinterpret_cast<ConnectionHandle>(event.peer);
                if (auto ip = peerIpString(event.peer->address)) {
                    e.peerIp = *ip;
                }
                e.peerPort = event.peer->address.port;
                outEvents.push_back(std::move(e));
                break;
            }
            case ENET_EVENT_TYPE_RECEIVE: {
                Event e;
                e.type = Event::Type::Receive;
                e.connection = reinterpret_cast<ConnectionHandle>(event.peer);
                e.payload.resize(event.packet->dataLength);
                std::memcpy(e.payload.data(), event.packet->data, event.packet->dataLength);
                outEvents.push_back(std::move(e));
                enet_packet_destroy(event.packet);
                break;
            }
            case ENET_EVENT_TYPE_DISCONNECT: {
                Event e;
                e.type = Event::Type::Disconnect;
                e.connection = reinterpret_cast<ConnectionHandle>(event.peer);
                if (auto ip = peerIpString(event.peer->address)) {
                    e.peerIp = *ip;
                }
                e.peerPort = event.peer->address.port;
                outEvents.push_back(std::move(e));
                break;
            }
            case ENET_EVENT_TYPE_DISCONNECT_TIMEOUT: {
                Event e;
                e.type = Event::Type::DisconnectTimeout;
                e.connection = reinterpret_cast<ConnectionHandle>(event.peer);
                if (auto ip = peerIpString(event.peer->address)) {
                    e.peerIp = *ip;
                }
                e.peerPort = event.peer->address.port;
                outEvents.push_back(std::move(e));
                break;
            }
            default:
                break;
            }
        }
    }

    void send(ConnectionHandle connection, const std::byte *data, std::size_t size, Delivery delivery, bool flush) override {
        if (!host) {
            return;
        }

        auto *peer = reinterpret_cast<ENetPeer*>(connection);
        if (!peer) {
            return;
        }

        ENetPacket *packet = enet_packet_create(data, size, toEnetFlag(delivery));
        enet_peer_send(peer, toEnetChannel(delivery, channelCount), packet);

        if (flush) {
            enet_host_flush(host);
        }
    }

    void disconnect(ConnectionHandle connection) override {
        auto *peer = reinterpret_cast<ENetPeer*>(connection);
        if (!peer) {
            return;
        }

        enet_peer_disconnect(peer, 0);
    }

private:
    EnetGlobal global;
    ENetHost *host = nullptr;
    int channelCount = 1;
};

} // namespace

std::unique_ptr<IClientTransport> createEnetClientTransport() {
    return std::make_unique<EnetClientTransport>();
}

std::unique_ptr<IServerTransport> createEnetServerTransport(uint16_t port, int maxClients, int numChannels) {
    return std::make_unique<EnetServerTransport>(port, maxClients, numChannels);
}

} // namespace net
