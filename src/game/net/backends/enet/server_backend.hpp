#pragma once

#include "game/net/backend.hpp"
#include "engine/network/transport.hpp"

#include <map>
#include <string>
#include <vector>

namespace game::net {

class EnetServerBackend final : public ServerBackend {
public:
    EnetServerBackend(uint16_t port, int maxClients, int numChannels);
    ~EnetServerBackend() override;

    void update() override;
    void flushPeekedMessages() override;
    void sendImpl(client_id clientId, const ServerMsg& input, bool flush) override;
    void disconnectClient(client_id clientId, const std::string& reason) override;
    std::vector<client_id> getClients() const override;

    std::vector<ServerMsgData>& receivedMessages() override { return receivedMessages_; }

private:
    client_id getClient(::net::ConnectionHandle connection);
    client_id getNextClientId();
    void logUnsupportedMessageType();

    std::unique_ptr<::net::IServerTransport> transport_;
    std::map<client_id, ::net::ConnectionHandle> clients_;
    std::map<::net::ConnectionHandle, client_id> clientByConnection_;
    std::map<::net::ConnectionHandle, std::string> ipByConnection_;
    std::vector<ServerMsgData> receivedMessages_;
};

} // namespace game::net
