#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace bz::data {

// Resolve paths located under the runtime data directory.
std::filesystem::path Resolve(const std::filesystem::path &relativePath);

// Overrides the detected data directory. Must be called before the first Resolve/DataRoot invocation.
void SetDataRootOverride(const std::filesystem::path &path);

std::optional<nlohmann::json> LoadJsonFile(const std::filesystem::path &path,
										   const std::string &label,
										   spdlog::level::level_enum missingLevel);

std::filesystem::path UserConfigDirectory();
std::filesystem::path EnsureUserConfigFile(const std::string &fileName);
std::filesystem::path EnsureUserWorldsDirectory();
std::filesystem::path EnsureUserWorldDirectoryForServer(const std::string &host, uint16_t port);
bool MergeConfigLayer(const std::string &label,
					 const nlohmann::json &layerJson,
					 const std::filesystem::path &baseDir);
bool MergeExternalConfigLayer(const std::filesystem::path &configPath,
								 const std::string &label = {},
								 spdlog::level::level_enum missingLevel = spdlog::level::warn);

struct ConfigLayerSpec {
	std::filesystem::path relativePath;
	std::string label;
	spdlog::level::level_enum missingLevel = spdlog::level::warn;
	bool required = false;
};

struct ConfigLayer {
	nlohmann::json json;
	std::filesystem::path baseDir;
	std::string label;
};

std::vector<ConfigLayer> LoadConfigLayers(const std::vector<ConfigLayerSpec> &specs);

void MergeJsonObjects(nlohmann::json &destination, const nlohmann::json &source);

void CollectAssetEntries(const nlohmann::json &node,
						 const std::filesystem::path &baseDir,
						 std::map<std::string, std::filesystem::path> &assetMap,
						 const std::string &prefix = "");

// Resolve an asset path declared in client/config.json, falling back to a default relative path.
std::filesystem::path ResolveConfiguredAsset(const std::string &assetKey,
											 const std::filesystem::path &defaultRelativePath = {});

// Returns the detected runtime data directory.
const std::filesystem::path &DataRoot();

// Initializes the global configuration cache using the provided layered specification list.
void InitializeConfigCache(const std::vector<ConfigLayerSpec> &specs);

// Returns true if the global configuration cache has been populated.
bool ConfigCacheInitialized();

// Retrieves the merged configuration hierarchy.
const nlohmann::json &ConfigCacheRoot();

// Retrieves the configuration JSON object for a named layer (if available).
const nlohmann::json *ConfigLayerByLabel(const std::string &label);

// Retrieves a configuration value from the merged cache using dotted path syntax.
const nlohmann::json *ConfigValue(const std::string &path);

// Returns a copy of the configuration value at the given path, if present.
std::optional<nlohmann::json> ConfigValueCopy(const std::string &path);

// Returns the configuration value at the given path interpreted as uint16_t, if possible.
std::optional<uint16_t> ConfigValueUInt16(const std::string &path);

// Returns the configuration value at the given path if it is a string.
std::optional<std::string> ConfigValueString(const std::string &path);

} // namespace bz::data
