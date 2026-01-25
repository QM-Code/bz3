#pragma once

#include "network/transport.hpp"

#include <cstdint>
#include <memory>

namespace net {

std::unique_ptr<IClientTransport> createDefaultClientTransport();
std::unique_ptr<IServerTransport> createDefaultServerTransport(uint16_t port, int maxClients = 50, int numChannels = 2);

} // namespace net
