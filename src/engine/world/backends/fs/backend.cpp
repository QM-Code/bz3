#include "world/backends/fs/backend.hpp"

#include "common/data_path_resolver.hpp"
#include "common/config_helpers.hpp"
#include "spdlog/spdlog.h"

#include <fstream>
#include <miniz.h>
#include <cstring>
#include <stdexcept>

namespace fs = std::filesystem;

namespace {
void ZipDirectory(const fs::path& inputDir, const fs::path& outputZip) {
    if (!fs::exists(inputDir) || !fs::is_directory(inputDir)) {
        throw std::runtime_error("Input is not a directory");
    }

    mz_zip_archive zip{};
    if (!mz_zip_writer_init_file(&zip, outputZip.string().c_str(), 0)) {
        throw std::runtime_error("Failed to create zip file");
    }

    try {
        for (const auto& entry : fs::recursive_directory_iterator(inputDir)) {
            if (!entry.is_regular_file()) {
                continue;
            }

            fs::path absPath = entry.path();
            fs::path relPath = fs::relative(absPath, inputDir);
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

world::ArchiveBytes ReadArchiveFile(const fs::path& zipPath) {
    if (!fs::exists(zipPath)) {
        throw std::runtime_error("World zip file not found: " + zipPath.string());
    }

    auto fileSize = fs::file_size(zipPath);
    world::ArchiveBytes data(fileSize);
    std::ifstream file(zipPath, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Failed to open zip file: " + zipPath.string());
    }

    file.read(reinterpret_cast<char*>(data.data()), fileSize);
    if (!file) {
        throw std::runtime_error("Failed to read zip file: " + zipPath.string());
    }

    return data;
}
}

namespace world_backend {

world::WorldContent FsWorldBackend::loadContent(const std::vector<bz::data::ConfigLayerSpec>& baseSpecs,
                                                const std::optional<bz::json::Value>& worldConfig,
                                                const fs::path& worldDir,
                                                const std::string& fallbackName,
                                                const std::string& logContext) {
    world::WorldContent content;
    content.rootDir = worldDir;
    content.name = fallbackName;

    std::vector<bz::data::ConfigLayer> layers = bz::data::LoadConfigLayers(baseSpecs);
    if (worldConfig.has_value()) {
        if (worldConfig->is_object()) {
            layers.push_back({*worldConfig, worldDir});
        } else {
            spdlog::warn("{}: World config for {} is not an object", logContext, worldDir.string());
        }
    }

    bz::json::Value mergedConfig = bz::json::Object();
    for (const auto& layer : layers) {
        bz::data::MergeJsonObjects(mergedConfig, layer.json);
        content.mergeLayer(layer.json, layer.baseDir);
    }

    content.config = std::move(mergedConfig);
    if (content.name.empty()) {
        content.name = fs::path(content.rootDir).filename().string();
    }
    spdlog::info("{}: Loaded world '{}'", logContext, content.name);
    return content;
}

world::ArchiveBytes FsWorldBackend::buildArchive(const fs::path& worldDir) {
    const fs::path inputDir(worldDir);
    fs::path outputZip = inputDir;
    outputZip += ".zip";
    ZipDirectory(inputDir, outputZip);
    return ReadArchiveFile(outputZip);
}

bool FsWorldBackend::extractArchive(const world::ArchiveBytes& data, const fs::path& destDir) {
    mz_zip_archive zip;
    memset(&zip, 0, sizeof(zip));

    if (!mz_zip_reader_init_mem(&zip, data.data(), data.size(), 0)) {
        spdlog::error("WorldArchive: Failed to open zip from memory");
        return false;
    }

    int numFiles = mz_zip_reader_get_num_files(&zip);

    for (int i = 0; i < numFiles; i++) {
        mz_zip_archive_file_stat fileStat;
        if (!mz_zip_reader_file_stat(&zip, i, &fileStat)) {
            spdlog::error("WorldArchive: Failed to get file stat for index {}", i);
            mz_zip_reader_end(&zip);
            return false;
        }

        std::string outPath = (destDir / fileStat.m_filename).string();

        if (mz_zip_reader_is_file_a_directory(&zip, i)) {
            fs::create_directories(outPath);
        } else {
            fs::create_directories(fs::path(outPath).parent_path());

            if (!mz_zip_reader_extract_to_file(&zip, i, outPath.c_str(), 0)) {
                spdlog::error("WorldArchive: Failed to extract: {}", fileStat.m_filename);
                mz_zip_reader_end(&zip);
                return false;
            }
        }
    }

    mz_zip_reader_end(&zip);
    spdlog::info("WorldArchive: Unzipped {} files to {}", numFiles, destDir.string());
    return true;
}

std::optional<bz::json::Value> FsWorldBackend::readJsonFile(const fs::path& path) {
    if (!fs::exists(path)) {
        return std::nullopt;
    }

    std::ifstream file(path);
    if (!file) {
        return std::nullopt;
    }

    try {
        bz::json::Value data;
        file >> data;
        return data;
    } catch (const std::exception& e) {
        spdlog::error("WorldArchive: Failed to parse JSON {}: {}", path.string(), e.what());
        return std::nullopt;
    }
}

} // namespace world_backend
