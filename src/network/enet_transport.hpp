#pragma once

#include "network/transport.hpp"

#include <memory>

namespace net {

std::unique_ptr<IClientTransport> createEnetClientTransport();
std::unique_ptr<IServerTransport> createEnetServerTransport(uint16_t port, int maxClients = 50, int numChannels = 2);

} // namespace net
