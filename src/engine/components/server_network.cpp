#include "engine/components/server_network.hpp"
#include "spdlog/spdlog.h"

#include "network/proto_codec.hpp"
#include "network/transport_factory.hpp"

#include <algorithm>

ServerNetwork::ServerNetwork(uint16_t port, int maxClients, int numChannels) {
    transport = net::createDefaultServerTransport(port, maxClients, numChannels);
    if (!transport) {
        spdlog::error("ServerNetwork::ServerNetwork: Failed to initialize server transport.");
        return;
    }

    spdlog::info("Server started on port {}", port);
}

ServerNetwork::~ServerNetwork() {
    for (auto &msgData : receivedMessages) {
        delete msgData.msg;
    }
    receivedMessages.clear();
    clients.clear();
    clientByConnection.clear();
    transport.reset();
}

void ServerNetwork::flushPeekedMessages() {
    receivedMessages.erase(
        std::remove_if(
            receivedMessages.begin(),
            receivedMessages.end(),
            [](const MsgData& msgData) {
                if (msgData.peeked) {
                    delete msgData.msg;
                }
                return msgData.peeked;
            }
        ),
        receivedMessages.end()
    );
}

client_id ServerNetwork::getClient(net::ConnectionHandle connection) {
    auto it = clientByConnection.find(connection);
    if (it != clientByConnection.end()) {
        return it->second;
    }

    spdlog::warn("ServerNetwork::getClient: Connection not found in client map.");
    return static_cast<client_id>(0); // Invalid client_id
}

client_id ServerNetwork::getNextClientId() {
    client_id id = FIRST_CLIENT_ID;
    while (clients.find(id) != clients.end()) {
        ++id;
    }
    return id;
}

void ServerNetwork::update() {
    if (!transport) {
        return;
    }

    std::vector<net::Event> events;
    transport->poll(events);

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
                    auto itIp = ipByConnection.find(evt.connection);
                    if (itIp != ipByConnection.end()) {
                        join->ip = itIp->second;
                    }
                }
            }

            receivedMessages.push_back({ decoded.release() });
            
            break;
        }
        case net::Event::Type::Connect: {
            client_id newClientId = getNextClientId();
            clients[newClientId] = evt.connection;
            clientByConnection[evt.connection] = newClientId;
            ipByConnection[evt.connection] = evt.peerIp;
            break;
        }
        case net::Event::Type::Disconnect:
        case net::Event::Type::DisconnectTimeout: {
            auto it = clientByConnection.find(evt.connection);
            if (it == clientByConnection.end()) {
                break;
            }
            client_id discClientId = it->second;
            clientByConnection.erase(it);
            clients.erase(discClientId);
            ipByConnection.erase(evt.connection);
            ClientMsg_PlayerLeave* discMsg = new ClientMsg_PlayerLeave();
            discMsg->clientId = discClientId;
            receivedMessages.push_back({ discMsg });
            break;
        }
        default:
            break;
        }
    }
}

std::vector<client_id> ServerNetwork::getClients() const {
    std::vector<client_id> clientIds;
    for (const auto& [id, peer] : clients) {
        clientIds.push_back(id);
    }
    return clientIds;
}

void ServerNetwork::disconnectClient(client_id clientId, const std::string &reason) {
    auto it = clients.find(clientId);
    if (it == clients.end()) {
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
    if (transport) {
        transport->disconnect(it->second);
    }
}

void ServerNetwork::logUnsupportedMessageType() {
    spdlog::error("ServerNetwork::send: Unsupported message type");
}

void ServerNetwork::sendImpl(client_id clientId, const ServerMsg &input, bool flush) {
    if (!transport) {
        return;
    }

    auto it = clients.find(clientId);
    if (it == clients.end()) {
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
    transport->send(it->second, encoded->data(), encoded->size(), delivery, shouldFlush);
}
