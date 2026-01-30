#include "game/net/server_network.hpp"

ServerNetwork::ServerNetwork(uint16_t port, int maxClients, int numChannels) {
    backend_ = game::net::CreateServerBackend(port, maxClients, numChannels);
}

ServerNetwork::~ServerNetwork() = default;

void ServerNetwork::flushPeekedMessages() {
    if (backend_) {
        backend_->flushPeekedMessages();
    }
}

void ServerNetwork::update() {
    if (backend_) {
        backend_->update();
    }
}

void ServerNetwork::sendImpl(client_id clientId, const ServerMsg &input, bool flush) {
    if (backend_) {
        backend_->sendImpl(clientId, input, flush);
    }
}

void ServerNetwork::disconnectClient(client_id clientId, const std::string &reason) {
    if (backend_) {
        backend_->disconnectClient(clientId, reason);
    }
}

std::vector<client_id> ServerNetwork::getClients() const {
    if (!backend_) {
        return {};
    }
    return backend_->getClients();
}
