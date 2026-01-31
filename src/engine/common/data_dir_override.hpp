#pragma once

#include <filesystem>
#include <optional>

namespace karma::data {

struct DataDirOverrideResult {
    std::filesystem::path userConfigPath;
    std::optional<std::filesystem::path> dataDir;
};

DataDirOverrideResult ApplyDataDirOverrideFromArgs(int argc, char *argv[], const std::filesystem::path &defaultConfigRelative = std::filesystem::path("config.json"));

} // namespace karma::data
