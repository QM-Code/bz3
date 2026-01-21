#pragma once

#include <chrono>
#include <memory>
#include <string>

#include <cstdint>
#include <nlohmann/json.hpp>

class Game;
class HeartbeatClient;

class CommunityHeartbeat {
public:
    CommunityHeartbeat();
    ~CommunityHeartbeat();
    void configureFromConfig(const nlohmann::json &mergedConfig,
                             uint16_t listenPort,
                             const std::string &communityOverride);
    void update(const Game &game);

private:
    std::unique_ptr<HeartbeatClient> client;
    bool enabled = false;
    int intervalSeconds = 0;
    std::string communityUrl;
    std::string serverAddress;
    int maxPlayers = 0;
    std::chrono::steady_clock::time_point nextHeartbeatTime{};
};
