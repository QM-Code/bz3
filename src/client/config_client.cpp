#include "client/config_client.hpp"
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include "spdlog/spdlog.h"
#include "common/data_path_resolver.hpp"

namespace {

ClientConfig ParseClientConfig(const nlohmann::json &root) {
    ClientConfig config;

    if (auto it = root.find("tankPath"); it != root.end() && it->is_string()) {
        config.tankPath = it->get<std::string>();
    }

    if (auto serverListsIt = root.find("serverLists"); serverListsIt != root.end()) {
        if (!serverListsIt->is_object()) {
            spdlog::warn("ClientConfig::Load: 'serverLists' must be an object");
        } else {
            const auto &serverListsObject = *serverListsIt;

            if (auto showLanIt = serverListsObject.find("showLAN"); showLanIt != serverListsObject.end() && showLanIt->is_boolean()) {
                config.showLanServers = showLanIt->get<bool>();
            }

            if (auto defaultIt = serverListsObject.find("default"); defaultIt != serverListsObject.end() && defaultIt->is_string()) {
                config.defaultServerList = defaultIt->get<std::string>();
            }

            if (auto sourcesIt = serverListsObject.find("sources"); sourcesIt != serverListsObject.end()) {
                if (!sourcesIt->is_array()) {
                    spdlog::warn("ClientConfig::Load: 'sources' must be an array");
                } else {
                    for (const auto &entry : *sourcesIt) {
                        if (!entry.is_object()) {
                            continue;
                        }

                        ClientServerListSource source;
                        if (auto nameIt = entry.find("name"); nameIt != entry.end() && nameIt->is_string()) {
                            source.name = nameIt->get<std::string>();
                        }

                        if (auto urlIt = entry.find("url"); urlIt != entry.end() && urlIt->is_string()) {
                            source.url = urlIt->get<std::string>();
                        }

                        if (!source.url.empty()) {
                            config.serverLists.push_back(source);
                        } else {
                            spdlog::warn("ClientConfig::Load: Skipping server list entry without URL");
                        }
                    }
                }
            }
        }
    }

    return config;
}

ClientConfig LoadClientConfigFromFiles(const std::filesystem::path &defaultConfigPath,
                                       const std::filesystem::path &userConfigPath) {
    nlohmann::json merged = nlohmann::json::object();

    if (auto defaults = bz::data::LoadJsonFile(defaultConfigPath, "client defaults", spdlog::level::warn)) {
        if (!defaults->is_object()) {
            spdlog::warn("ClientConfig::Load: {} is not a JSON object", defaultConfigPath.string());
        } else {
            bz::data::MergeJsonObjects(merged, *defaults);
        }
    }

    if (auto user = bz::data::LoadJsonFile(userConfigPath, "user config", spdlog::level::debug)) {
        if (!user->is_object()) {
            spdlog::warn("ClientConfig::Load: User config at {} is not a JSON object", userConfigPath.string());
        } else {
            bz::data::MergeJsonObjects(merged, *user);
        }
    }

    return ParseClientConfig(merged);
}

} // namespace

ClientConfig ClientConfig::Load(const std::string &path) {
    if (!path.empty()) {
        const std::filesystem::path defaultConfigPath(path);
        const std::filesystem::path userConfigPath = bz::data::EnsureUserConfigFile("config.json");
        return LoadClientConfigFromFiles(defaultConfigPath, userConfigPath);
    }

    if (!bz::data::ConfigCacheInitialized()) {
        spdlog::debug("ClientConfig::Load: Config cache uninitialized; falling back to direct file load");
        const auto defaultConfigPath = bz::data::Resolve("client/config.json");
        const auto userConfigPath = bz::data::EnsureUserConfigFile("config.json");
        return LoadClientConfigFromFiles(defaultConfigPath, userConfigPath);
    }

    const auto &root = bz::data::ConfigCacheRoot();
    if (!root.is_object()) {
        spdlog::warn("ClientConfig::Load: Configuration cache root is not a JSON object");
        return ClientConfig{};
    }

    return ParseClientConfig(root);
}

bool ClientConfig::Save(const std::string &path) const {
    const std::filesystem::path filePath(path);

    nlohmann::json userConfig = nlohmann::json::object();

    {
        std::ifstream file(filePath);
        if (file.is_open()) {
            try {
                file >> userConfig;
                if (!userConfig.is_object()) {
                    spdlog::warn("ClientConfig::Save: Existing {} is not a JSON object; overwriting", path);
                    userConfig = nlohmann::json::object();
                }
            } catch (const std::exception &ex) {
                spdlog::warn("ClientConfig::Save: Failed to parse existing {}: {}", path, ex.what());
                userConfig = nlohmann::json::object();
            }
        }
    }

    if (!tankPath.empty()) {
        userConfig["tankPath"] = tankPath;
    } else {
        userConfig.erase("tankPath");
    }

    nlohmann::json serverListsObject = nlohmann::json::object();
    serverListsObject["showLAN"] = showLanServers;
    if (!defaultServerList.empty()) {
        serverListsObject["default"] = defaultServerList;
    } else {
        serverListsObject.erase("default");
    }

    nlohmann::json sourcesArray = nlohmann::json::array();
    for (const auto &source : serverLists) {
        if (source.url.empty()) {
            continue;
        }

        nlohmann::json entry;
        entry["url"] = source.url;
        if (!source.name.empty()) {
            entry["name"] = source.name;
        }
        sourcesArray.push_back(std::move(entry));
    }

    serverListsObject["sources"] = std::move(sourcesArray);
    userConfig["serverLists"] = std::move(serverListsObject);

    std::error_code ec;
    const auto parentDir = filePath.parent_path();
    if (!parentDir.empty()) {
        std::filesystem::create_directories(parentDir, ec);
        if (ec) {
            spdlog::warn("ClientConfig::Save: Failed to create directory {}: {}", parentDir.string(), ec.message());
        }
    }

    std::ofstream file(filePath, std::ios::trunc);
    if (!file.is_open()) {
        spdlog::warn("ClientConfig::Save: Unable to open {} for writing", path);
        return false;
    }

    try {
        file << userConfig.dump(4) << '\n';
    } catch (const std::exception &ex) {
        spdlog::warn("ClientConfig::Save: Failed to write {}: {}", path, ex.what());
        return false;
    }

    return true;
}
