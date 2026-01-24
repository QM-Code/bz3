#pragma once
#include <filesystem>
#include <map>
#include <string>
#include <vector>
#include "core/types.hpp"
#include <nlohmann/json.hpp>
#include "physics/static_body.hpp"

class Game;

class World {
private:
    Game &game;
    render_id renderId;
    PhysicsStaticBody physics;
    PlayerParameters defaultPlayerParams;
    bool initialized = false;

    std::string worldDir;
    std::string serverName;
    std::string worldName;
    uint32_t protocolVersion = 0;
    std::vector<std::string> features;
    std::map<std::string, std::filesystem::path> assetPaths;

    void registerAssets(const nlohmann::json &source, const std::filesystem::path &baseDir);
    bool unzipFromMemory(const std::byte* data, size_t size, const std::string& destDir);

public:
    client_id playerId;

    World(Game &game, std::string worldDir);
    ~World();

    void load(std::string worldPath);
    bool isInitialized() const;
    void update();
    std::filesystem::path getAssetPath(const std::string &assetName) const;

    PlayerParameters getDefaultPlayerParameters() const {
        return defaultPlayerParams;
    }
};
