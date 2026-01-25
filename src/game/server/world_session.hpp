#pragma once

#include "core/types.hpp"
#include "game/net/messages.hpp"
#include "physics/static_body.hpp"
#include "game/world/config.hpp"
#include "world/backend.hpp"
#include "world/content.hpp"

#include <filesystem>
#include <memory>
#include "common/json.hpp"
#include <string>
#include <vector>

class Game;

class ServerWorldSession {
private:
    Game &game;
    std::unique_ptr<world_backend::Backend> backend_;

    std::string serverName;
    world::WorldContent content_;
    PlayerParameters defaultPlayerParameters_;

    PhysicsStaticBody physics;
    bool archiveOnStartup = true;
    bool archiveCached = false;
    world::ArchiveBytes archiveCache;

    world::ArchiveBytes buildArchive();

public:
    ServerWorldSession(Game &game,
                       std::string serverName,
                       std::string worldName,
                       bz::json::Value worldConfig,
                       std::string worldDir,
                       bool enableWorldZipping);
    ~ServerWorldSession();

    void update();
    void sendWorldInit(client_id clientId);

    std::filesystem::path resolveAssetPath(const std::string &assetName) const;
    const bz::json::Value &config() const { return content_.config; }
    const PlayerParameters &defaultPlayerParameters() const { return defaultPlayerParameters_; }

    Location pickSpawnLocation() const;
};
