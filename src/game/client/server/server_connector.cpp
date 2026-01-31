#include "client/server/server_connector.hpp"

#include <utility>

#include "game/engine/client_engine.hpp"
#include "client/game.hpp"
#include "karma/common/config_helpers.hpp"
#include "spdlog/spdlog.h"

ServerConnector::ServerConnector(ClientEngine &engine,
                                 std::string playerName,
                                 std::string worldDir,
                                 std::unique_ptr<Game> &game)
    : engine(engine),
      game(game),
      defaultPlayerName(std::move(playerName)),
      worldDir(std::move(worldDir)) {}

bool ServerConnector::connect(const std::string &targetHost,
                              uint16_t targetPort,
                              const std::string &playerName,
                              bool registeredUser,
                              bool communityAdmin,
                              bool localAdmin) {
    std::string status = "Connecting to " + targetHost + ":" + std::to_string(targetPort) + "...";
    auto &browser = engine.ui->console();
    browser.setStatus(status, false);
    spdlog::info("Attempting to connect to {}:{}", targetHost, targetPort);

    const std::string resolvedName = playerName.empty() ? defaultPlayerName : playerName;
    const uint16_t connectTimeoutMs = karma::config::ReadUInt16Config({"network.ConnectTimeoutMs"}, 2000);
    if (engine.network->connect(targetHost, targetPort, static_cast<int>(connectTimeoutMs))) {
        spdlog::info("Connected to server at {}:{}", targetHost, targetPort);
        spdlog::info("Join mode: {} user", registeredUser ? "registered" : "anonymous");
        spdlog::info("Join flags: community_admin={}, local_admin={}", communityAdmin, localAdmin);
        browser.setConnectionState({true, targetHost, targetPort});
        game = std::make_unique<Game>(engine, resolvedName, worldDir, registeredUser, communityAdmin, localAdmin);
        spdlog::trace("Game initialized successfully");

        ClientMsg_PlayerJoin joinMsg{};
        joinMsg.clientId = 0; // server will overwrite based on connection
        joinMsg.name = resolvedName;
        joinMsg.protocolVersion = NET_PROTOCOL_VERSION;
        joinMsg.ip = ""; // client does not know its external IP
        joinMsg.registeredUser = registeredUser;
        joinMsg.communityAdmin = communityAdmin;
        joinMsg.localAdmin = localAdmin;
        engine.network->send<ClientMsg_PlayerJoin>(joinMsg);

        browser.hide();
        return true;
    }

    spdlog::error("Failed to connect to server at {}:{}", targetHost, targetPort);
    std::string errorMsg = "Unable to reach " + targetHost + ":" + std::to_string(targetPort) + ".";
    browser.setStatus(errorMsg, true);
    browser.setConnectionState({});
    return false;
}
