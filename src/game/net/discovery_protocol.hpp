#pragma once
#include <cstdint>

namespace DiscoveryProtocol {

constexpr uint32_t MAGIC = 0x425a4449; // 'BZDI'
constexpr uint16_t VERSION = 1;
constexpr uint16_t PORT = 47800;

enum class PacketType : uint16_t {
    Request = 1,
    Response = 2
};

#pragma pack(push, 1)
struct PacketHeader {
    uint32_t magic = MAGIC;
    uint16_t version = VERSION;
    uint16_t type;
};

struct RequestPacket : PacketHeader {
    RequestPacket() {
        type = static_cast<uint16_t>(PacketType::Request);
    }
};

struct ResponsePacket : PacketHeader {
    ResponsePacket() {
        type = static_cast<uint16_t>(PacketType::Response);
        serverName[0] = '\0';
        worldName[0] = '\0';
        gamePort = 0;
        serverId = 0;
    }

    uint16_t gamePort;   // network byte order
    uint32_t serverId;   // unique per server instance
    char serverName[64];
    char worldName[64];
};
#pragma pack(pop)

inline bool isValid(const PacketHeader &header) {
    return header.magic == MAGIC && header.version == VERSION;
}

inline bool isRequest(const PacketHeader &header) {
    return isValid(header) && header.type == static_cast<uint16_t>(PacketType::Request);
}

inline bool isResponse(const PacketHeader &header) {
    return isValid(header) && header.type == static_cast<uint16_t>(PacketType::Response);
}

} // namespace DiscoveryProtocol
