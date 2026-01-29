#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "common/json.hpp"
#include <spdlog/spdlog.h>

namespace bz::config {

struct ConfigFileSpec {
    std::filesystem::path path;
    std::string label;
    spdlog::level::level_enum missingLevel = spdlog::level::warn;
    bool required = false;
    bool resolveRelativeToDataRoot = true;
};

struct ConfigLayer {
    bz::json::Value json;
    std::filesystem::path baseDir;
    std::string label;
};

class ConfigStore {
public:
    static void Initialize(const std::vector<ConfigFileSpec> &defaultSpecs,
                           const std::filesystem::path &userConfigPath,
                           const std::vector<ConfigFileSpec> &runtimeSpecs = {});
    static bool Initialized();
    static uint64_t Revision();

    static const bz::json::Value &Defaults();
    static const bz::json::Value &User();
    static const bz::json::Value &Merged();

    static const bz::json::Value *Get(std::string_view path);
    static std::optional<bz::json::Value> GetCopy(std::string_view path);

    static bool Set(std::string_view path, bz::json::Value value);
    static bool Erase(std::string_view path);
    static bool ReplaceUserConfig(bz::json::Value userConfig, std::string *error = nullptr);

    static bool SaveUser(std::string *error = nullptr);
    static const std::filesystem::path &UserConfigPath();

    static bool AddRuntimeLayer(const std::string &label,
                                const bz::json::Value &layerJson,
                                const std::filesystem::path &baseDir);
    static bool RemoveRuntimeLayer(const std::string &label);
    static const bz::json::Value *LayerByLabel(const std::string &label);

    static std::filesystem::path ResolveAssetPath(const std::string &assetKey,
                                                  const std::filesystem::path &defaultPath = {});

private:
    static void rebuildMergedLocked();
    static bool setValueAtPath(bz::json::Value &root, std::string_view path, bz::json::Value value);
    static bool eraseValueAtPath(bz::json::Value &root, std::string_view path);
    static bool saveUserUnlocked(std::string *error);
};

} // namespace bz::config
