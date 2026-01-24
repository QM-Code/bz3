#pragma once

#include "core/types.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace network_backend {

struct DisconnectEvent {
    std::string reason;
};

struct ServerEndpointInfo {
    std::string host;
    uint16_t port = 0;
};

struct ClientMsgData {
    ServerMsg* msg = nullptr;
    bool peeked = false;
};

struct ServerMsgData {
    ClientMsg* msg = nullptr;
    bool peeked = false;
};

class ClientBackend {
public:
    virtual ~ClientBackend() = default;

    virtual bool connect(const std::string& address, uint16_t port, int timeoutMs) = 0;
    virtual void disconnect(const std::string& reason) = 0;
    virtual std::optional<DisconnectEvent> consumeDisconnectEvent() = 0;
    virtual bool isConnected() const = 0;
    virtual std::optional<ServerEndpointInfo> getServerEndpoint() const = 0;

    virtual void update() = 0;
    virtual void flushPeekedMessages() = 0;
    virtual void sendImpl(const ClientMsg& input, bool flush) = 0;

    virtual std::vector<ClientMsgData>& receivedMessages() = 0;
};

class ServerBackend {
public:
    virtual ~ServerBackend() = default;

    virtual void update() = 0;
    virtual void flushPeekedMessages() = 0;
    virtual void sendImpl(client_id clientId, const ServerMsg& input, bool flush) = 0;
    virtual void disconnectClient(client_id clientId, const std::string& reason) = 0;
    virtual std::vector<client_id> getClients() const = 0;

    virtual std::vector<ServerMsgData>& receivedMessages() = 0;
};

std::unique_ptr<ClientBackend> CreateClientBackend();
std::unique_ptr<ServerBackend> CreateServerBackend(uint16_t port, int maxClients, int numChannels);

} // namespace network_backend
