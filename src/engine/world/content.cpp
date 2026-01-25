#include "world/content.hpp"

#include "common/data_path_resolver.hpp"
#include "spdlog/spdlog.h"

namespace {
std::string LeafKey(const std::string& key) {
    const auto separator = key.find_last_of('.');
    return separator == std::string::npos ? key : key.substr(separator + 1);
}
}

namespace world {

void AssetCatalog::mergeFromJson(const bz::json::Value& assetsJson, const std::filesystem::path& baseDir) {
    if (!assetsJson.is_object()) {
        return;
    }

    std::map<std::string, std::filesystem::path> collected;
    bz::data::CollectAssetEntries(assetsJson, baseDir, collected);

    for (const auto& [key, path] : collected) {
        entries[key] = path;
        entries[LeafKey(key)] = path;
    }
}

std::optional<std::filesystem::path> AssetCatalog::findPath(const std::string& key) const {
    auto it = entries.find(key);
    if (it == entries.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::filesystem::path AssetCatalog::resolvePath(const std::string& key, const char* logContext) const {
    auto resolved = findPath(key);
    if (resolved.has_value()) {
        return resolved.value();
    }
    spdlog::error("{}: Asset '{}' not found", logContext, key);
    return {};
}

void WorldContent::mergeLayer(const bz::json::Value& layerJson, const std::filesystem::path& baseDir) {
    if (!layerJson.is_object()) {
        return;
    }

    const auto assetsIt = layerJson.find("assets");
    if (assetsIt != layerJson.end()) {
        if (!assetsIt->is_object()) {
            spdlog::warn("WorldContent: 'assets' in layer is not an object; skipping");
        } else {
            assets.mergeFromJson(*assetsIt, baseDir);
        }
    }
}

std::filesystem::path WorldContent::resolveAssetPath(const std::string& key, const char* logContext) const {
    return assets.resolvePath(key, logContext);
}

} // namespace world
