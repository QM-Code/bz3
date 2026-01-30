#include "game/net/backend.hpp"

#if defined(KARMA_NETWORK_BACKEND_ENET)
#include "game/net/backends/enet/client_backend.hpp"
#include "game/net/backends/enet/server_backend.hpp"
#else
#error "BZ3 network backend not set. Define KARMA_NETWORK_BACKEND_ENET."
#endif

namespace game::net {

std::unique_ptr<ClientBackend> CreateClientBackend() {
#if defined(KARMA_NETWORK_BACKEND_ENET)
    return std::make_unique<EnetClientBackend>();
#endif
}

std::unique_ptr<ServerBackend> CreateServerBackend(uint16_t port, int maxClients, int numChannels) {
#if defined(KARMA_NETWORK_BACKEND_ENET)
    return std::make_unique<EnetServerBackend>(port, maxClients, numChannels);
#endif
}

} // namespace game::net
