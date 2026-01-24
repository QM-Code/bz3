#include "network/backends/enet/server_backend.hpp"

#include "network/proto_codec.hpp"
#include "network/transport_factory.hpp"
#include "spdlog/spdlog.h"

#include <algorithm>

namespace network_backend {

EnetServerBackend::EnetServerBackend(uint16_t port, int maxClients, int numChannels) {
    transport_ = net::createDefaultServerTransport(port, maxClients, numChannels);
    if (!transport_) {
        spdlog::error("ServerNetwork::ServerNetwork: Failed to initialize server transport.");
        return;
    }

    spdlog::info("Server started on port {}", port);
}

EnetServerBackend::~EnetServerBackend() {
    for (auto &msgData : receivedMessages_) {
        delete msgData.msg;
    }
    receivedMessages_.clear();
    clients_.clear();
    clientByConnection_.clear();
    transport_.reset();
}

void EnetServerBackend::flushPeekedMessages() {
    receivedMessages_.erase(
        std::remove_if(
            receivedMessages_.begin(),
            receivedMessages_.end(),
            [](const ServerMsgData& msgData) {
                if (msgData.peeked) {
                    delete msgData.msg;
                }
                return msgData.peeked;
            }
        ),
        receivedMessages_.end()
    );
}

client_id EnetServerBackend::getClient(net::ConnectionHandle connection) {
    auto it = clientByConnection_.find(connection);
    if (it != clientByConnection_.end()) {
        return it->second;
    }

    spdlog::warn("ServerNetwork::getClient: Connection not found in client map.");
    return static_cast<client_id>(0); // Invalid client_id
}

client_id EnetServerBackend::getNextClientId() {
    client_id id = FIRST_CLIENT_ID;
    while (clients_.find(id) != clients_.end()) {
        ++id;
    }
    return id;
}

void EnetServerBackend::update() {
    if (!transport_) {
        return;
    }

    std::vector<net::Event> events;
    transport_->poll(events);

    for (const auto &evt : events) {
        switch (evt.type) {
        case net::Event::Type::Receive: {
            if (evt.payload.empty()) {
                break;
            }

            auto decoded = net::decodeClientMsg(evt.payload.data(), evt.payload.size());
            if (!decoded) {
                spdlog::warn("Received unknown/invalid ClientMsg payload");
                break;
            }

            const client_id cid = getClient(evt.connection);
            if (cid == 0) {
                break;
            }

            decoded->clientId = cid;

            if (decoded->type == ClientMsg_Type_PLAYER_JOIN) {
                auto *join = static_cast<ClientMsg_PlayerJoin*>(decoded.get());
                // Prefer transport-reported IP if client left it blank
                if (join->ip.empty()) {
                    auto itIp = ipByConnection_.find(evt.connection);
                    if (itIp != ipByConnection_.end()) {
                        join->ip = itIp->second;
                    }
                }
            }

            receivedMessages_.push_back({ decoded.release() });

            break;
        }
        case net::Event::Type::Connect: {
            client_id newClientId = getNextClientId();
            clients_[newClientId] = evt.connection;
            clientByConnection_[evt.connection] = newClientId;
            ipByConnection_[evt.connection] = evt.peerIp;
            break;
        }
        case net::Event::Type::Disconnect:
        case net::Event::Type::DisconnectTimeout: {
            auto it = clientByConnection_.find(evt.connection);
            if (it == clientByConnection_.end()) {
                break;
            }
            client_id discClientId = it->second;
            clientByConnection_.erase(it);
            clients_.erase(discClientId);
            ipByConnection_.erase(evt.connection);
            ClientMsg_PlayerLeave* discMsg = new ClientMsg_PlayerLeave();
            discMsg->clientId = discClientId;
            receivedMessages_.push_back({ discMsg });
            break;
        }
        default:
            break;
        }
    }
}

std::vector<client_id> EnetServerBackend::getClients() const {
    std::vector<client_id> clientIds;
    for (const auto& [id, peer] : clients_) {
        clientIds.push_back(id);
    }
    return clientIds;
}

void EnetServerBackend::disconnectClient(client_id clientId, const std::string &reason) {
    auto it = clients_.find(clientId);
    if (it == clients_.end()) {
        spdlog::warn("ServerNetwork::disconnectClient: Attempted to disconnect unknown client {}", clientId);
        return;
    }

    if (!reason.empty()) {
        ServerMsg_Chat notice;
        notice.fromId = SERVER_CLIENT_ID;
        notice.toId = clientId;
        notice.text = reason;
        sendImpl(clientId, notice, true);
    }

    spdlog::info("ServerNetwork::disconnectClient: Disconnecting client {}", clientId);
    if (transport_) {
        transport_->disconnect(it->second);
    }
}

void EnetServerBackend::logUnsupportedMessageType() {
    spdlog::error("ServerNetwork::send: Unsupported message type");
}

void EnetServerBackend::sendImpl(client_id clientId, const ServerMsg &input, bool flush) {
    if (!transport_) {
        return;
    }

    auto it = clients_.find(clientId);
    if (it == clients_.end()) {
        return;
    }

    net::Delivery delivery = net::Delivery::Reliable;
    if (input.type == ServerMsg_Type_PLAYER_LOCATION) {
        delivery = net::Delivery::Unreliable;
    }

    auto encoded = net::encodeServerMsg(input);
    if (!encoded.has_value()) {
        logUnsupportedMessageType();
        return;
    }

    const bool shouldFlush = flush || (input.type == ServerMsg_Type_INIT);
    transport_->send(it->second, encoded->data(), encoded->size(), delivery, shouldFlush);
}

} // namespace network_backend
