#pragma once

#include "world/content.hpp"

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace bz::data {
struct ConfigLayerSpec;
}

namespace world_backend {

class Backend {
public:
    virtual ~Backend() = default;

    virtual world::WorldContent loadContent(const std::vector<bz::data::ConfigLayerSpec>& baseSpecs,
                                            const std::optional<bz::json::Value>& worldConfig,
                                            const std::filesystem::path& worldDir,
                                            const std::string& fallbackName,
                                            const std::string& logContext) = 0;

    virtual world::ArchiveBytes buildArchive(const std::filesystem::path& worldDir) = 0;
    virtual bool extractArchive(const world::ArchiveBytes& data, const std::filesystem::path& destDir) = 0;
    virtual std::optional<bz::json::Value> readJsonFile(const std::filesystem::path& path) = 0;
};

std::unique_ptr<Backend> CreateWorldBackend();

} // namespace world_backend
