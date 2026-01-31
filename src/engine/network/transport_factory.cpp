#include "network/transport_factory.hpp"

#include "network/enet_transport.hpp"
#include "spdlog/spdlog.h"

namespace net {

std::unique_ptr<IClientTransport> createDefaultClientTransport() {
    return createEnetClientTransport();
}

std::unique_ptr<IServerTransport> createDefaultServerTransport(uint16_t port, int maxClients, int numChannels) {
    if (numChannels != 2) {
        spdlog::warn("Server transport requested {} channels; forcing 2 for default transport compatibility", numChannels);
        numChannels = 2;
    }
    return createEnetServerTransport(port, maxClients, numChannels);
}

} // namespace net
