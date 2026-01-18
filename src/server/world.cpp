#include "world.hpp"
#include "game.hpp"
#include "spdlog/spdlog.h"
#include "common/data_path_resolver.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>
#include <fstream>
#include <map>
#include <random>
#include <stdexcept>
#include <string>
#include <utility>
#include <miniz.h>
#include <vector>

namespace fs = std::filesystem;

World::World(Game &game,
             std::string serverName,
             std::string worldName,
             nlohmann::json worldConfig,
             std::string worldDir,
             bool enableWorldZipping)
        : game(game),
          serverName(std::move(serverName)),
          name(std::move(worldName)),
          worldDir(std::move(worldDir)),
          zipWorldOnStartup(enableWorldZipping) {
    const std::vector<bz::data::ConfigLayerSpec> baseSpecs = {
        {"common/config.json", "data/common/config.json", spdlog::level::err, true},
        {"server/config.json", "data/server/config.json", spdlog::level::err, true}
    };

    std::vector<bz::data::ConfigLayer> layers = bz::data::LoadConfigLayers(baseSpecs);

    if (worldConfig.is_object()) {
        layers.push_back({std::move(worldConfig), fs::path(this->worldDir)});
    } else {
        spdlog::warn("World::World: World config for {} is not an object", this->worldDir);
    }

    nlohmann::json mergedConfig = nlohmann::json::object();
    for (const auto &layer : layers) {
        bz::data::MergeJsonObjects(mergedConfig, layer.json);

        if (layer.json.is_object()) {
            const auto assetsIt = layer.json.find("assets");
            if (assetsIt != layer.json.end()) {
                if (!assetsIt->is_object()) {
                    spdlog::warn("World::World: 'assets' in layer is not an object; skipping");
                } else {
                    std::map<std::string, std::filesystem::path> layerAssets;
                    bz::data::CollectAssetEntries(*assetsIt, layer.baseDir, layerAssets);

                    for (const auto &[assetKey, assetPath] : layerAssets) {
                        assetPaths[assetKey] = assetPath;

                        const auto separator = assetKey.find_last_of('.');
                        if (separator != std::string::npos) {
                            assetPaths[assetKey.substr(separator + 1)] = assetPath;
                        }
                    }
                }
            }
        }

        if (layer.json.contains("defaultPlayerParameters") && layer.json["defaultPlayerParameters"].is_object()) {
            for (const auto &[key, value] : layer.json["defaultPlayerParameters"].items()) {
                if (value.is_number()) {
                    defaultPlayerParams[key] = value.get<float>();
                }
            }
        }
    }

    config = std::move(mergedConfig);
    
    if (name.empty()) {
        name = fs::path(this->worldDir).filename().string();
    }
    spdlog::info("World::World: Loaded world '{}'", name);

    if (zipWorldOnStartup) {
        // Create a zip file of the directory "worldDir", and save it at the same location
        // with the same name but with .zip extension
        fs::path inputDir(this->worldDir);
        fs::path outputZip = inputDir;
        outputZip += ".zip";
        zipDirectory(inputDir, outputZip);
        worldDataCache = getData();
        worldDataCached = true;
    } else {
        spdlog::debug("World::World: Skipping zip generation for bundled world at {}", this->worldDir);
    }

    physics = game.engine.physics->createStaticMesh(getAssetPath("world"));
}

World::~World() {
    physics.destroy();
}

void World::zipDirectory(const fs::path& inputDir, const fs::path& outputZip) {
    if (!fs::exists(inputDir) || !fs::is_directory(inputDir)) {
        throw std::runtime_error("Input is not a directory");
    }

    mz_zip_archive zip{};
    if (!mz_zip_writer_init_file(&zip, outputZip.string().c_str(), 0)) {
        throw std::runtime_error("Failed to create zip file");
    }

    try {
        for (const auto& entry : fs::recursive_directory_iterator(inputDir)) {
            if (!entry.is_regular_file()) continue;

            fs::path absPath = entry.path();
            fs::path relPath = fs::relative(absPath, inputDir);

            // ZIP requires forward slashes
            std::string zipPath = relPath.generic_string();

            if (!mz_zip_writer_add_file(
                    &zip,
                    zipPath.c_str(),
                    absPath.string().c_str(),
                    nullptr,
                    0,
                    MZ_DEFAULT_LEVEL)) {
                throw std::runtime_error("Failed to add file: " + zipPath);
            }
        }

        if (!mz_zip_writer_finalize_archive(&zip)) {
            throw std::runtime_error("Failed to finalize zip");
        }

        mz_zip_writer_end(&zip);
    } catch (...) {
        mz_zip_writer_end(&zip);
        throw;
    }
}

std::vector<std::byte> World::getData() {
    if (!zipWorldOnStartup) {
        return {};
    }

    if (worldDataCached) {
        return worldDataCache;
    }

    // Construct the zip path from the world name
    fs::path zipPath = fs::path(this->worldDir);
    zipPath += ".zip";

    if (!fs::exists(zipPath)) {
        throw std::runtime_error("World zip file not found: " + zipPath.string());
    }

    // Get file size
    auto fileSize = fs::file_size(zipPath);

    // Read the entire zip file into memory
    std::vector<std::byte> data(fileSize);
    std::ifstream file(zipPath, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Failed to open zip file: " + zipPath.string());
    }

    file.read(reinterpret_cast<char*>(data.data()), fileSize);
    if (!file) {
        throw std::runtime_error("Failed to read zip file: " + zipPath.string());
    }

    worldDataCache = data;
    worldDataCached = true;
    return worldDataCache;
}

void World::update() {
}

void World::sendInitToClient(client_id clientId) {
    std::vector<std::byte> worldData = getData();

    ServerMsg_Init initHeaderMsg;
    initHeaderMsg.clientId = clientId;
    initHeaderMsg.serverName = serverName;
    initHeaderMsg.worldName = name;
    initHeaderMsg.protocolVersion = NET_PROTOCOL_VERSION;
    initHeaderMsg.defaultPlayerParams = defaultPlayerParams;
    initHeaderMsg.worldData = worldData;
    game.engine.network->send<ServerMsg_Init>(clientId, &initHeaderMsg);

    spdlog::trace("World::sendInitToClient: Sent init message to client id {}", clientId);
}

std::string World::getAssetPath(const std::string &assetName) const {
    auto it = assetPaths.find(assetName);
    if (it != assetPaths.end()) {
        return it->second.string();
    }

    spdlog::error("World::getAssetPath: Asset '{}' not found", assetName);
    return "";
}

Location World::getSpawnLocation() const {
    static thread_local std::mt19937 rng{std::random_device{}()};
    std::uniform_real_distribution<float> distXZ(-20.0f, 20.0f);
    std::uniform_real_distribution<float> distRot(0.0f, glm::two_pi<float>());

    const float x = distXZ(rng);
    const float z = distXZ(rng);

    const glm::vec3 rayStart{x, 500.0f, z};
    const glm::vec3 rayEnd{x, -100.0f, z};
    glm::vec3 hitPoint{};
    glm::vec3 hitNormal{};
    float y = 5.0f; // fallback height if nothing is hit

    if (game.engine.physics && game.engine.physics->raycast(rayStart, rayEnd, hitPoint, hitNormal)) {
        y = hitPoint.y;
    }

    const float rotY = distRot(rng);
    return Location{
        .position = glm::vec3(x, y, z),
        .rotation = glm::angleAxis(rotY, glm::vec3(0.0f, 1.0f, 0.0f))
    };
}
