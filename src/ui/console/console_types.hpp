#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ui {

struct CommunityBrowserEntry {
    std::string label;
    std::string host;
    uint16_t port;
    std::string description;
    std::string displayHost;
    std::string longDescription;
    std::string code;
    std::vector<std::string> flags;
    int activePlayers = -1;
    int maxPlayers = -1;
    std::string gameMode;
    std::string screenshotId;
    std::string sourceHost;
    std::string worldName;
};

struct CommunityBrowserSelection {
    std::string host;
    uint16_t port;
    bool fromPreset;
    std::string sourceHost;
    std::string worldName;
};

struct ServerListOption {
    std::string name;
    std::string host;
};

enum class MessageTone {
    Notice,
    Error,
    Pending
};

} // namespace ui
