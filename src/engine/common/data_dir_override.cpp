#include "common/data_dir_override.hpp"

#include "common/data_path_resolver.hpp"
#include "common/json.hpp"
#include <spdlog/spdlog.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <system_error>
#include <stdexcept>

namespace {

std::optional<std::filesystem::path> ParsePathArg(int argc, char *argv[], const std::string &shortOpt, const std::string &longOpt) {
    const std::string longPrefix = longOpt + "=";

    for (int i = 1; i < argc; ++i) {
        const std::string arg(argv[i]);
        if (arg == shortOpt || arg == longOpt) {
            if (i + 1 < argc) {
                return std::filesystem::path(argv[i + 1]);
            }
            break;
        }

        if (arg.rfind(longPrefix, 0) == 0) {
            return std::filesystem::path(arg.substr(longPrefix.size()));
        }
    }

    return std::nullopt;
}

std::filesystem::path CanonicalizePath(const std::filesystem::path &path) {
    std::error_code ec;
    auto canonical = std::filesystem::weakly_canonical(path, ec);
    if (!ec) {
        return canonical;
    }
    canonical = std::filesystem::absolute(path, ec);
    return ec ? path : canonical;
}

std::filesystem::path EnsureConfigFileAtPath(const std::filesystem::path &path, const std::filesystem::path &defaultRelative) {
    if (path.empty()) {
        // Build a path under the user config directory respecting relative subfolders (e.g., server/config.json).
        const auto baseDir = bz::data::UserConfigDirectory();
        const auto target = baseDir / defaultRelative;

        const auto parent = target.parent_path();
        if (!parent.empty()) {
            std::error_code dirEc;
            std::filesystem::create_directories(parent, dirEc);
            if (dirEc) {
                throw std::runtime_error("Failed to create config directory " + parent.string() + ": " + dirEc.message());
            }
        }

        if (!std::filesystem::exists(target)) {
            std::ofstream stream(target);
            if (!stream) {
                throw std::runtime_error("Failed to create user config file " + target.string());
            }

            stream << "{}\n";
            if (!stream) {
                throw std::runtime_error("Failed to initialize user config file " + target.string());
            }
        } else if (std::filesystem::is_regular_file(target)) {
            std::error_code sizeEc;
            const auto fileSize = std::filesystem::file_size(target, sizeEc);
            if (!sizeEc && fileSize == 0) {
                std::ofstream stream(target, std::ios::trunc);
                if (!stream) {
                    throw std::runtime_error("Failed to truncate empty user config file " + target.string());
                }

                stream << "{}\n";
                if (!stream) {
                    throw std::runtime_error("Failed to initialize truncated user config file " + target.string());
                }
            }
        }

        return CanonicalizePath(target);
    }

    const auto parent = path.parent_path();
    if (!parent.empty()) {
        std::error_code dirEc;
        std::filesystem::create_directories(parent, dirEc);
        if (dirEc) {
            throw std::runtime_error("Failed to create config directory " + parent.string() + ": " + dirEc.message());
        }
    }

    if (!std::filesystem::exists(path)) {
        std::ofstream stream(path);
        if (!stream) {
            throw std::runtime_error("Failed to create user config file " + path.string());
        }

        stream << "{}\n";
        if (!stream) {
            throw std::runtime_error("Failed to initialize user config file " + path.string());
        }
    } else if (std::filesystem::is_regular_file(path)) {
        std::error_code sizeEc;
        const auto fileSize = std::filesystem::file_size(path, sizeEc);
        if (!sizeEc && fileSize == 0) {
            std::ofstream stream(path, std::ios::trunc);
            if (!stream) {
                throw std::runtime_error("Failed to truncate empty user config file " + path.string());
            }

            stream << "{}\n";
            if (!stream) {
                throw std::runtime_error("Failed to initialize truncated user config file " + path.string());
            }
        }
    }

    return CanonicalizePath(path);
}

std::optional<std::filesystem::path> ExtractDataDirFromConfig(const std::filesystem::path &configPath) {
    std::ifstream stream(configPath);
    if (!stream) {
        // If the file cannot be opened, fall back to other mechanisms.
        return std::nullopt;
    }

    try {
        bz::json::Value configJson;
        stream >> configJson;

        if (!configJson.is_object()) {
            return std::nullopt;
        }

        if (auto dataDirIt = configJson.find("DataDir"); dataDirIt != configJson.end() && dataDirIt->is_string()) {
            const auto value = dataDirIt->get<std::string>();
            if (!value.empty()) {
                return std::filesystem::path(value);
            }
        }
    } catch (const std::exception &ex) {
        throw std::runtime_error("Failed to parse user config at " + configPath.string() + ": " + ex.what());
    }

    return std::nullopt;
}

bool IsValidDir(const std::filesystem::path &path) {
    std::error_code ec;
    return std::filesystem::exists(path, ec) && std::filesystem::is_directory(path, ec);
}

void ValidateDataDirOrExit(const std::filesystem::path &path, const std::string &sourceLabel, const std::optional<std::filesystem::path> &configPath = std::nullopt) {
    if (!IsValidDir(path)) {
        std::cerr << "Invalid data directory specified: \"" << sourceLabel << "\"\n";
        std::cerr << "" << path.string() << " does not exist or is not a directory.\n";
        if (configPath) {
            std::cerr << "User config path: " << configPath->string() << "\n";
        }
        std::exit(1);
    }

    std::error_code ec;
    const auto commonConfig = path / "common" / "config.json";
    if (!std::filesystem::exists(commonConfig, ec) || !std::filesystem::is_regular_file(commonConfig, ec)) {
        std::cerr << "Invalid data directory specified: \"" << sourceLabel << "\"\n";
        std::cerr << commonConfig.string() << " does not exist." << std::endl;
        if (configPath) {
            std::cerr << "User config path: " << configPath->string() << std::endl;
        }
        std::exit(1);
    }
}

} // namespace

namespace bz::data {

DataDirOverrideResult ApplyDataDirOverrideFromArgs(int argc, char *argv[], const std::filesystem::path &defaultConfigRelative) {
    try {
        const auto cliConfigPath = ParsePathArg(argc, argv, "-c", "--config");
        const auto cliDataDir = ParsePathArg(argc, argv, "-d", "--data-dir");

        const std::filesystem::path configPath = EnsureConfigFileAtPath(cliConfigPath ? *cliConfigPath : std::filesystem::path{}, defaultConfigRelative);
        const auto configDataDir = cliDataDir ? std::optional<std::filesystem::path>{} : ExtractDataDirFromConfig(configPath);

        if (cliDataDir) {
            ValidateDataDirOrExit(*cliDataDir, std::string("-d ") + cliDataDir->string());
            bz::data::SetDataRootOverride(*cliDataDir);
            spdlog::debug("Using data directory from CLI override: {}", cliDataDir->string());
            return {configPath, *cliDataDir};
        }

        if (configDataDir) {
            ValidateDataDirOrExit(*configDataDir, std::string("user config"), configPath);
            bz::data::SetDataRootOverride(*configDataDir);
            spdlog::debug("Using data directory from user config: {}", configDataDir->string());
            return {configPath, *configDataDir};
        }

        // Fall back to BZ3_DATA_DIR if present; otherwise fail with a friendly message.
        if (const char *envDataDir = std::getenv("BZ3_DATA_DIR"); envDataDir && *envDataDir) {
            const std::filesystem::path envPath(envDataDir);
            ValidateDataDirOrExit(envPath, std::string("BZ3_DATA_DIR: ") + envDataDir);
            bz::data::SetDataRootOverride(envPath);
            spdlog::debug("Using data directory from BZ3_DATA_DIR: {}", envPath.string());
            return {configPath, envPath};
        }

        std::cerr << "\n";
        std::cerr << "The bz3 data directory could not be found.\n";
        std::cerr << "\n";
        std::cerr << "This should not happen and may indicate a problem with installation.\n\n";
        std::cerr << "This directory can be specified in three ways:\n";
        std::cerr << "  1. Set the BZ3_DATA_DIR environment variable.\n";
        std::cerr << "  2. Use the command-line option \"-d <datadir>\".\n";
        std::cerr << "  3. Add the following to your config file:\n";
        std::cerr << "     " << configPath.string() << "\n";
        std::cerr << "     {\n";
        std::cerr << "         \"DataDir\" : \"<datadir>\"\n";
        std::cerr << "     }\n";
        std::cerr << "\n";
        std::exit(1);
    } catch (const std::exception &ex) {
        std::cerr << ex.what() << std::endl;
        std::exit(1);
    }
}

} // namespace bz::data
