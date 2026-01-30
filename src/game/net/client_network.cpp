#include "game/net/client_network.hpp"

ClientNetwork::ClientNetwork() {
    backend_ = game::net::CreateClientBackend();
}

ClientNetwork::~ClientNetwork() = default;

void ClientNetwork::flushPeekedMessages() {
    if (backend_) {
        backend_->flushPeekedMessages();
    }
}

void ClientNetwork::update() {
    if (backend_) {
        backend_->update();
    }
}

bool ClientNetwork::connect(const std::string &addr, uint16_t port, int timeoutMs) {
    if (!backend_) {
        return false;
    }
    return backend_->connect(addr, port, timeoutMs);
}

void ClientNetwork::disconnect(const std::string &reason) {
    if (backend_) {
        backend_->disconnect(reason);
    }
}

std::optional<ClientNetwork::DisconnectEvent> ClientNetwork::consumeDisconnectEvent() {
    if (!backend_) {
        return std::nullopt;
    }
    return backend_->consumeDisconnectEvent();
}

bool ClientNetwork::isConnected() const {
    return backend_ && backend_->isConnected();
}

std::optional<ClientNetwork::ServerEndpointInfo> ClientNetwork::getServerEndpoint() const {
    if (!backend_) {
        return std::nullopt;
    }
    return backend_->getServerEndpoint();
}

void ClientNetwork::sendImpl(const ClientMsg &input, bool flush) {
    if (backend_) {
        backend_->sendImpl(input, flush);
    }
}
