#include "client/world_session.hpp"

#include "client/game.hpp"
#include "spdlog/spdlog.h"
#include "common/data_path_resolver.hpp"
#include "common/config_helpers.hpp"

ClientWorldSession::ClientWorldSession(Game &game, std::string worldDir)
        : game(game), backend_(world_backend::CreateWorldBackend()) {
    const auto userConfigPath = bz::data::EnsureUserConfigFile("config.json");

    const std::vector<bz::data::ConfigLayerSpec> layerSpecs = {
        {"common/config.json", "data/common/config.json", spdlog::level::err, true},
        {"client/config.json", "data/client/config.json", spdlog::level::debug, false},
        {userConfigPath, "user config", spdlog::level::debug, false}
    };

    content_ = backend_->loadContent(layerSpecs,
                                     std::nullopt,
                                     std::filesystem::path(worldDir),
                                     std::string{},
                                     "ClientWorldSession");
}

ClientWorldSession::~ClientWorldSession() {
    game.engine.render->destroy(renderId);
    physics.destroy();
}

void ClientWorldSession::load(std::string worldPath) {
    content_.rootDir = std::move(worldPath);
}

bool ClientWorldSession::isInitialized() const {
    return initialized;
}

void ClientWorldSession::update() {
    for (const auto &initMsg : game.engine.network->consumeMessages<ServerMsg_Init>()) {
        spdlog::trace("ClientWorldSession: Received init message from server");
        serverName = initMsg.serverName;
        content_.name = initMsg.worldName;
        protocolVersion = initMsg.protocolVersion;
        features = initMsg.features;
        if (protocolVersion != 0 && protocolVersion != NET_PROTOCOL_VERSION) {
            spdlog::error("ClientWorldSession: Protocol version mismatch (client {}, server {})",
                          NET_PROTOCOL_VERSION,
                          protocolVersion);
            game.engine.network->disconnect("Protocol version mismatch.");
            return;
        }
        for (const auto& [key, val] : initMsg.defaultPlayerParams) {
            content_.defaultPlayerParameters[key] = val;
        }
        playerId = initMsg.clientId;

        if (!initMsg.worldData.empty()) {
            std::filesystem::path downloadsDir;
            if (const auto endpoint = game.engine.network->getServerEndpoint()) {
                downloadsDir = bz::data::EnsureUserWorldDirectoryForServer(endpoint->host, endpoint->port);
            } else {
                spdlog::warn("ClientWorldSession: Server endpoint unknown; falling back to shared world directory");
                downloadsDir = bz::data::EnsureUserWorldsDirectory();
            }

            content_.rootDir = downloadsDir;

            backend_->extractArchive(initMsg.worldData, downloadsDir);

            const auto worldConfigPath = downloadsDir / "config.json";
            auto worldConfigOpt = backend_->readJsonFile(worldConfigPath);
            if (worldConfigOpt.has_value()) {
                if (!worldConfigOpt->is_object()) {
                    spdlog::warn("ClientWorldSession: World config is not a JSON object: {}", worldConfigPath.string());
                } else {
                    constexpr const char* worldConfigLabel = "world config";
                    if (!bz::data::MergeConfigLayer(worldConfigLabel, *worldConfigOpt, downloadsDir)) {
                        spdlog::warn("ClientWorldSession: Failed to merge world config layer from {}", worldConfigPath.string());
                    } else {
                        bz::data::MergeJsonObjects(content_.config, *worldConfigOpt);
                        content_.mergeLayer(*worldConfigOpt, downloadsDir);
                    }
                }
            } else {
                spdlog::warn("ClientWorldSession: World config not found at {}", worldConfigPath.string());
            }

        } else {
            spdlog::debug("ClientWorldSession: Received bundled world indication; skipping download");
        }

        renderId = game.engine.render->create(resolveAssetPath("world").string());
        physics = game.engine.physics->createStaticMesh(resolveAssetPath("world").string());

        spdlog::info("ClientWorldSession: World initialized from server");
        initialized = true;
        return;
    }
}

std::filesystem::path ClientWorldSession::resolveAssetPath(const std::string &assetName) const {
    return content_.resolveAssetPath(assetName, "ClientWorldSession");
}
