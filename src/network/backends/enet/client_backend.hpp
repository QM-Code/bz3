#pragma once

#include "network/backend.hpp"
#include "network/transport.hpp"

#include <optional>
#include <vector>

namespace network_backend {

class EnetClientBackend final : public ClientBackend {
public:
    EnetClientBackend();
    ~EnetClientBackend() override;

    bool connect(const std::string& address, uint16_t port, int timeoutMs) override;
    void disconnect(const std::string& reason) override;
    std::optional<DisconnectEvent> consumeDisconnectEvent() override;
    bool isConnected() const override;
    std::optional<ServerEndpointInfo> getServerEndpoint() const override;

    void update() override;
    void flushPeekedMessages() override;
    void sendImpl(const ClientMsg& input, bool flush) override;

    std::vector<ClientMsgData>& receivedMessages() override { return receivedMessages_; }

private:
    void logUnsupportedMessageType();

    std::unique_ptr<net::IClientTransport> transport_;
    std::optional<DisconnectEvent> pendingDisconnect_;
    std::optional<ServerEndpointInfo> serverEndpoint_;
    std::vector<ClientMsgData> receivedMessages_;
};

} // namespace network_backend
