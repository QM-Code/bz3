#pragma once

#include "world/backend.hpp"

namespace world_backend {

class FsWorldBackend final : public Backend {
public:
    world::WorldContent loadContent(const std::vector<bz::data::ConfigLayerSpec>& baseSpecs,
                                    const std::optional<bz::json::Value>& worldConfig,
                                    const std::filesystem::path& worldDir,
                                    const std::string& fallbackName,
                                    const std::string& logContext) override;

    world::ArchiveBytes buildArchive(const std::filesystem::path& worldDir) override;
    bool extractArchive(const world::ArchiveBytes& data, const std::filesystem::path& destDir) override;
    std::optional<bz::json::Value> readJsonFile(const std::filesystem::path& path) override;
};

} // namespace world_backend
