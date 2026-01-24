#include "network/backends/enet/client_backend.hpp"

#include "network/proto_codec.hpp"
#include "network/transport_factory.hpp"
#include "spdlog/spdlog.h"

#include <algorithm>

namespace {
constexpr const char* kDisconnectReason = "Disconnected from server.";
constexpr const char* kTimeoutReason = "Connection lost (timeout).";
}

namespace network_backend {

EnetClientBackend::EnetClientBackend() {
    transport_ = net::createDefaultClientTransport();
}

EnetClientBackend::~EnetClientBackend() {
    for (auto &msgData : receivedMessages_) {
        delete msgData.msg;
    }
    receivedMessages_.clear();
    transport_.reset();
}

void EnetClientBackend::flushPeekedMessages() {
    receivedMessages_.erase(
        std::remove_if(
            receivedMessages_.begin(),
            receivedMessages_.end(),
            [](const ClientMsgData& msgData) {
                if (msgData.peeked) {
                    delete msgData.msg;
                }
                return msgData.peeked;
            }
        ),
        receivedMessages_.end()
    );
}

void EnetClientBackend::update() {
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

            auto decoded = net::decodeServerMsg(evt.payload.data(), evt.payload.size());
            if (!decoded) {
                spdlog::warn("Received unknown/invalid ServerMsg payload");
                break;
            }

            receivedMessages_.push_back({ decoded.release() });
            break;
        }
        case net::Event::Type::Disconnect: {
            spdlog::info("{}", kDisconnectReason);
            pendingDisconnect_ = DisconnectEvent{ kDisconnectReason };
            serverEndpoint_.reset();
            for (auto &msgData : receivedMessages_) {
                delete msgData.msg;
            }
            receivedMessages_.clear();
            break;
        }
        case net::Event::Type::DisconnectTimeout: {
            spdlog::info("{}", kTimeoutReason);
            pendingDisconnect_ = DisconnectEvent{ kTimeoutReason };
            serverEndpoint_.reset();
            for (auto &msgData : receivedMessages_) {
                delete msgData.msg;
            }
            receivedMessages_.clear();
            break;
        }
        default:
            break;
        }
    }
}

bool EnetClientBackend::connect(const std::string &addr, uint16_t port, int timeoutMs) {
    if (!transport_) {
        spdlog::error("ClientNetwork::connect: transport is not initialized.");
        return false;
    }

    pendingDisconnect_.reset();
    for (auto &msgData : receivedMessages_) {
        delete msgData.msg;
    }
    receivedMessages_.clear();

    if (!transport_->connect(addr, port, timeoutMs)) {
        spdlog::info("Connection to server failed.");
        serverEndpoint_.reset();
        return false;
    }

    spdlog::info("Connected to server.");
    auto remoteIp = transport_->getRemoteIp();
    auto remotePort = transport_->getRemotePort();
    serverEndpoint_ = ServerEndpointInfo{ remoteIp.value_or(addr), remotePort.value_or(port) };
    return true;
}

void EnetClientBackend::disconnect(const std::string &reason) {
    if (!transport_ || !transport_->isConnected()) {
        return;
    }

    transport_->disconnect();
    pendingDisconnect_ = DisconnectEvent{ reason.empty() ? kDisconnectReason : reason };
    serverEndpoint_.reset();
    for (auto &msgData : receivedMessages_) {
        delete msgData.msg;
    }
    receivedMessages_.clear();
}

std::optional<DisconnectEvent> EnetClientBackend::consumeDisconnectEvent() {
    if (!pendingDisconnect_.has_value()) {
        return std::nullopt;
    }
    auto evt = pendingDisconnect_;
    pendingDisconnect_.reset();
    return evt;
}

bool EnetClientBackend::isConnected() const {
    return transport_ && transport_->isConnected();
}

std::optional<ServerEndpointInfo> EnetClientBackend::getServerEndpoint() const {
    return serverEndpoint_;
}

void EnetClientBackend::logUnsupportedMessageType() {
    spdlog::error("ClientNetwork::send: Unsupported message type");
}

void EnetClientBackend::sendImpl(const ClientMsg &input, bool flush) {
    if (!transport_ || !transport_->isConnected()) {
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

    transport_->send(encoded->data(), encoded->size(), delivery, flush);
}

} // namespace network_backend
