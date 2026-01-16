#include "client/server/server_connector.hpp"

#include <utility>

#include "engine/client_engine.hpp"
#include "game.hpp"
#include "common/config_helpers.hpp"
#include "spdlog/spdlog.h"

ServerConnector::ServerConnector(ClientEngine &engine,
                                 std::string playerName,
                                 std::string worldDir,
                                 std::unique_ptr<Game> &game)
    : engine(engine),
      game(game),
      playerName(std::move(playerName)),
      worldDir(std::move(worldDir)) {}

bool ServerConnector::connect(const std::string &targetHost, uint16_t targetPort) {
    std::string status = "Connecting to " + targetHost + ":" + std::to_string(targetPort) + "...";
    auto &browser = engine.gui->serverBrowser();
    browser.setStatus(status, false);
    spdlog::info("Attempting to connect to {}:{}", targetHost, targetPort);

    const uint16_t connectTimeoutMs = bz::data::ReadUInt16Config({"network.ConnectTimeoutMs"}, 2000);
    if (engine.network->connect(targetHost, targetPort, static_cast<int>(connectTimeoutMs))) {
        spdlog::info("Connected to server at {}:{}", targetHost, targetPort);
        game = std::make_unique<Game>(engine, playerName, worldDir);
        spdlog::trace("Game initialized successfully");

        ClientMsg_PlayerJoin joinMsg{};
        joinMsg.clientId = 0; // server will overwrite based on connection
        joinMsg.name = playerName;
        joinMsg.protocolVersion = NET_PROTOCOL_VERSION;
        joinMsg.ip = ""; // client does not know its external IP
        engine.network->send<ClientMsg_PlayerJoin>(joinMsg);

        browser.hide();
        return true;
    }

    spdlog::error("Failed to connect to server at {}:{}", targetHost, targetPort);
    std::string errorMsg = "Unable to reach " + targetHost + ":" + std::to_string(targetPort) + ".";
    browser.setStatus(errorMsg, true);
    return false;
}
