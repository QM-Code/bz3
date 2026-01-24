#pragma once

#include "core/types.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace net {

std::unique_ptr<ServerMsg> decodeServerMsg(const std::byte *data, std::size_t size);
std::unique_ptr<ClientMsg> decodeClientMsg(const std::byte *data, std::size_t size);

std::optional<std::vector<std::byte>> encodeClientMsg(const ClientMsg &input);
std::optional<std::vector<std::byte>> encodeServerMsg(const ServerMsg &input);

} // namespace net
