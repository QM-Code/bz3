#include "server/community_heartbeat.hpp"

#include "server/heartbeat_client.hpp"
#include "server/game.hpp"
#include "common/config_helpers.hpp"
#include <spdlog/spdlog.h>

CommunityHeartbeat::CommunityHeartbeat() : client(std::make_unique<HeartbeatClient>()) {}

CommunityHeartbeat::~CommunityHeartbeat() = default;

void CommunityHeartbeat::configureFromConfig(const nlohmann::json &mergedConfig,
                                             uint16_t listenPort) {
    std::string advertiseHost = bz::data::ReadStringConfig("network.ServerAdvertiseHost", "");
    std::string serverHost = bz::data::ReadStringConfig("network.ServerHost", "");
    if (advertiseHost.empty() || advertiseHost == "0.0.0.0") {
        advertiseHost = serverHost;
    }
    if (advertiseHost == "0.0.0.0") {
        advertiseHost.clear();
    }
    if (advertiseHost.empty()) {
        serverAddress = std::to_string(listenPort);
        spdlog::warn("Community heartbeat will omit host; set network.ServerAdvertiseHost to advertise a host.");
    } else {
        serverAddress = advertiseHost + ":" + std::to_string(listenPort);
    }

    maxPlayers = 0;
    if (auto maxIt = mergedConfig.find("maxPlayers"); maxIt != mergedConfig.end() && maxIt->is_number_integer()) {
        maxPlayers = maxIt->get<int>();
    }

    communityUrl.clear();
    enabled = false;
    intervalSeconds = 0;

    if (auto it = mergedConfig.find("community"); it != mergedConfig.end() && it->is_object()) {
        const auto &community = *it;
        if (auto serverIt = community.find("server"); serverIt != community.end() && serverIt->is_string()) {
            communityUrl = serverIt->get<std::string>();
        }
        if (auto enabledIt = community.find("enabled"); enabledIt != community.end() && enabledIt->is_boolean()) {
            enabled = enabledIt->get<bool>();
        } else if (!communityUrl.empty()) {
            enabled = true;
        }
        if (auto intervalIt = community.find("heartbeatIntervalSeconds"); intervalIt != community.end()) {
            if (intervalIt->is_number_integer()) {
                intervalSeconds = intervalIt->get<int>();
            } else if (intervalIt->is_string()) {
                try {
                    intervalSeconds = std::stoi(intervalIt->get<std::string>());
                } catch (...) {
                    intervalSeconds = 0;
                }
            }
        }
    }

    if (communityUrl.empty()) {
        enabled = false;
    }

    nextHeartbeatTime = std::chrono::steady_clock::time_point{};
}

void CommunityHeartbeat::update(const Game &game) {
    if (!enabled || intervalSeconds <= 0 || communityUrl.empty()) {
        return;
    }

    auto now = std::chrono::steady_clock::now();
    if (nextHeartbeatTime == std::chrono::steady_clock::time_point{}) {
        nextHeartbeatTime = now + std::chrono::seconds(intervalSeconds);
        return;
    }

    if (now < nextHeartbeatTime) {
        return;
    }

    const int playerCount = static_cast<int>(game.getClients().size());
    client->requestHeartbeat(communityUrl, serverAddress, playerCount, maxPlayers);
    nextHeartbeatTime = now + std::chrono::seconds(intervalSeconds);
}
