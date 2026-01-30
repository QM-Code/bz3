#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "common/json.hpp"
#include <spdlog/spdlog.h>

namespace bz::data {

// Resolve paths located under the runtime data directory.
std::filesystem::path Resolve(const std::filesystem::path &relativePath);

// Overrides the detected data directory. Must be called before the first Resolve/DataRoot invocation.
void SetDataRootOverride(const std::filesystem::path &path);

std::optional<bz::json::Value> LoadJsonFile(const std::filesystem::path &path,
										   const std::string &label,
										   spdlog::level::level_enum missingLevel);

std::filesystem::path UserConfigDirectory();
std::filesystem::path EnsureUserConfigFile(const std::string &fileName);
std::filesystem::path EnsureUserWorldsDirectory();
std::filesystem::path EnsureUserWorldDirectoryForServer(const std::string &host, uint16_t port);

struct ConfigLayerSpec {
	std::filesystem::path relativePath;
	std::string label;
	spdlog::level::level_enum missingLevel = spdlog::level::warn;
	bool required = false;
};

struct ConfigLayer {
	bz::json::Value json;
	std::filesystem::path baseDir;
	std::string label;
};

std::vector<ConfigLayer> LoadConfigLayers(const std::vector<ConfigLayerSpec> &specs);

void MergeJsonObjects(bz::json::Value &destination, const bz::json::Value &source);

void CollectAssetEntries(const bz::json::Value &node,
						 const std::filesystem::path &baseDir,
						 std::map<std::string, std::filesystem::path> &assetMap,
						 const std::string &prefix = "");

struct DataPathSpec {
    std::string appName = "app";
    std::string dataDirEnvVar = "DATA_DIR";
    std::filesystem::path requiredDataMarker;
    std::vector<ConfigLayerSpec> fallbackAssetLayers;
};

void SetDataPathSpec(DataPathSpec spec);
DataPathSpec GetDataPathSpec();


// Returns the directory containing the running executable.
std::filesystem::path ExecutableDirectory();

// Resolve an asset path declared in configuration layers, falling back to a default relative path.
std::filesystem::path ResolveConfiguredAsset(const std::string &assetKey,
											 const std::filesystem::path &defaultRelativePath = {});
// Returns the detected runtime data directory.
const std::filesystem::path &DataRoot();

} // namespace bz::data
