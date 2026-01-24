#include "network/client_network.hpp"
#include "network/proto_codec.hpp"
#include "network/transport_factory.hpp"
#include "spdlog/spdlog.h"

#include <algorithm>

namespace {
constexpr const char* DISCONNECT_REASON = "Disconnected from server.";
constexpr const char* TIMEOUT_REASON = "Connection lost (timeout).";
}

ClientNetwork::ClientNetwork() {
    transport = net::createDefaultClientTransport();
}

ClientNetwork::~ClientNetwork() {
    for (auto &msgData : receivedMessages) {
        delete msgData.msg;
    }
    receivedMessages.clear();
    transport.reset();
}

void ClientNetwork::flushPeekedMessages() {
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

void ClientNetwork::update() {
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

            auto decoded = net::decodeServerMsg(evt.payload.data(), evt.payload.size());
            if (!decoded) {
                spdlog::warn("Received unknown/invalid ServerMsg payload");
                break;
            }

            receivedMessages.push_back({ decoded.release() });
            
            break;
        }
        case net::Event::Type::Disconnect: {
            spdlog::info("{}", DISCONNECT_REASON);
            pendingDisconnect = DisconnectEvent{ DISCONNECT_REASON };
            serverEndpoint.reset();
            for (auto &msgData : receivedMessages) {
                delete msgData.msg;
            }
            receivedMessages.clear();
            break;
        }
        case net::Event::Type::DisconnectTimeout: {
            spdlog::info("{}", TIMEOUT_REASON);
            pendingDisconnect = DisconnectEvent{ TIMEOUT_REASON };
            serverEndpoint.reset();
            for (auto &msgData : receivedMessages) {
                delete msgData.msg;
            }
            receivedMessages.clear();
            break;
        }
        default:
            break;
        }
    }
}

bool ClientNetwork::connect(const std::string &addr, uint16_t port, int timeoutMs) {
    if (!transport) {
        spdlog::error("ClientNetwork::connect: transport is not initialized.");
        return false;
    }

    pendingDisconnect.reset();
    for (auto &msgData : receivedMessages) {
        delete msgData.msg;
    }
    receivedMessages.clear();

    if (!transport->connect(addr, port, timeoutMs)) {
        spdlog::info("Connection to server failed.");
        serverEndpoint.reset();
        return false;
    }

    spdlog::info("Connected to server.");
    auto remoteIp = transport->getRemoteIp();
    auto remotePort = transport->getRemotePort();
    serverEndpoint = ServerEndpointInfo{ remoteIp.value_or(addr), remotePort.value_or(port) };
    return true;
}

void ClientNetwork::disconnect(const std::string &reason) {
    if (!transport || !transport->isConnected()) {
        return;
    }

    transport->disconnect();
    pendingDisconnect = DisconnectEvent{ reason.empty() ? DISCONNECT_REASON : reason };
    serverEndpoint.reset();
    for (auto &msgData : receivedMessages) {
        delete msgData.msg;
    }
    receivedMessages.clear();
}

std::optional<ClientNetwork::DisconnectEvent> ClientNetwork::consumeDisconnectEvent() {
    if (!pendingDisconnect.has_value()) {
        return std::nullopt;
    }
    auto evt = pendingDisconnect;
    pendingDisconnect.reset();
    return evt;
}

void ClientNetwork::logUnsupportedMessageType() {
    spdlog::error("ClientNetwork::send: Unsupported message type");
}

void ClientNetwork::sendImpl(const ClientMsg &input, bool flush) {
    if (!transport || !transport->isConnected()) {
        return;
    }

    net::Delivery delivery = net::Delivery::Reliable;
    if (input.type == ClientMsg_Type_PLAYER_LOCATION) {
        delivery = net::Delivery::Unreliable;
    }

    auto encoded = net::encodeClientMsg(input);
    if (!encoded.has_value()) {
        logUnsupportedMessageType();
        return;
    }

    transport->send(encoded->data(), encoded->size(), delivery, flush);
}
