#include "network/backend.hpp"

#if defined(BZ3_NETWORK_BACKEND_ENET)
#include "network/backends/enet/client_backend.hpp"
#include "network/backends/enet/server_backend.hpp"
#else
#error "BZ3 network backend not set. Define BZ3_NETWORK_BACKEND_ENET."
#endif

namespace network_backend {

std::unique_ptr<ClientBackend> CreateClientBackend() {
#if defined(BZ3_NETWORK_BACKEND_ENET)
    return std::make_unique<EnetClientBackend>();
#endif
}

std::unique_ptr<ServerBackend> CreateServerBackend(uint16_t port, int maxClients, int numChannels) {
#if defined(BZ3_NETWORK_BACKEND_ENET)
    return std::make_unique<EnetServerBackend>(port, maxClients, numChannels);
#endif
}

} // namespace network_backend
