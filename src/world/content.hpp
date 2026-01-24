#pragma once

#include "core/types.hpp"

#include <filesystem>
#include <map>
#include "common/json.hpp"
#include <optional>
#include <string>
#include <vector>

namespace world {

using ArchiveBytes = std::vector<std::byte>;

struct AssetCatalog {
    std::map<std::string, std::filesystem::path> entries;

    void mergeFromJson(const bz::json::Value& assetsJson, const std::filesystem::path& baseDir);
    std::filesystem::path resolvePath(const std::string& key, const char* logContext) const;
    std::optional<std::filesystem::path> findPath(const std::string& key) const;
};

struct WorldContent {
    std::string name;
    std::filesystem::path rootDir;
    bz::json::Value config;
    PlayerParameters defaultPlayerParameters;
    AssetCatalog assets;

    void mergeLayer(const bz::json::Value& layerJson, const std::filesystem::path& baseDir);
    std::filesystem::path resolveAssetPath(const std::string& key, const char* logContext) const;
};

} // namespace world
