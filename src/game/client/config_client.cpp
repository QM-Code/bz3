#include "client/config_client.hpp"
#include <filesystem>
#include <fstream>
#include "common/json.hpp"
#include "spdlog/spdlog.h"
#include "common/data_path_resolver.hpp"
#include "common/config_store.hpp"

namespace {

ClientConfig ParseClientConfig(const bz::json::Value &root) {
    ClientConfig config;

    auto parsePositiveInt = [](const bz::json::Value &object, const char *key) {
        int value = 0;
        auto it = object.find(key);
        if (it == object.end()) {
            return value;
        }

        try {
            if (it->is_number_integer()) {
                value = it->get<int>();
            } else if (it->is_string()) {
                value = std::stoi(it->get<std::string>());
            }
        } catch (...) {
            value = 0;
        }

        if (value <= 0) {
            return 0;
        }
        return value;
    };

    if (auto it = root.find("tankPath"); it != root.end() && it->is_string()) {
        config.tankPath = it->get<std::string>();
    }

    if (auto guiIt = root.find("gui"); guiIt != root.end() && guiIt->is_object()) {
        const auto &guiObject = *guiIt;
        (void)guiObject;
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

            auto parseCommunities = [&](const bz::json::Value &array) {
                for (const auto &entry : array) {
                    if (!entry.is_object()) {
                        continue;
                    }

                    ClientServerListSource source;
                    if (auto nameIt = entry.find("name"); nameIt != entry.end() && nameIt->is_string()) {
                        source.name = nameIt->get<std::string>();
                    }

                    if (auto hostIt = entry.find("host"); hostIt != entry.end() && hostIt->is_string()) {
                        source.host = hostIt->get<std::string>();
                    }

                    if (!source.host.empty()) {
                        config.serverLists.push_back(source);
                    } else {
                        spdlog::warn("ClientConfig::Load: Skipping server list entry without host");
                    }
                }
            };

            if (auto communitiesIt = serverListsObject.find("communities"); communitiesIt != serverListsObject.end()) {
                if (!communitiesIt->is_array()) {
                    spdlog::warn("ClientConfig::Load: 'communities' must be an array");
                } else {
                    parseCommunities(*communitiesIt);
                }
            } else if (auto sourcesIt = serverListsObject.find("sources"); sourcesIt != serverListsObject.end()) {
                // Backward compatibility: treat legacy 'sources' array as communities with 'url' fields.
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
                            source.host = urlIt->get<std::string>();
                        }

                        if (!source.host.empty()) {
                            config.serverLists.push_back(source);
                        } else {
                            spdlog::warn("ClientConfig::Load: Skipping server list entry without host");
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
    bz::json::Value merged = bz::json::Object();

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
    const auto userConfigPath = bz::config::ConfigStore::Initialized()
        ? bz::config::ConfigStore::UserConfigPath()
        : bz::data::EnsureUserConfigFile("config.json");

    if (!path.empty()) {
        const std::filesystem::path defaultConfigPath(path);
        return LoadClientConfigFromFiles(defaultConfigPath, userConfigPath);
    }

    if (!bz::config::ConfigStore::Initialized()) {
        spdlog::debug("ClientConfig::Load: Config cache uninitialized; falling back to direct file load");
        const auto defaultConfigPath = bz::data::Resolve("client/config.json");
        return LoadClientConfigFromFiles(defaultConfigPath, userConfigPath);
    }

    const auto &root = bz::config::ConfigStore::Merged();
    if (!root.is_object()) {
        spdlog::warn("ClientConfig::Load: Configuration cache root is not a JSON object");
        return ClientConfig{};
    }

    return ParseClientConfig(root);
}

bool ClientConfig::Save(const std::string &path) const {
    const std::filesystem::path filePath(path);

    bz::json::Value userConfig = bz::json::Object();

    {
        std::ifstream file(filePath);
        if (file.is_open()) {
            try {
                file >> userConfig;
                if (!userConfig.is_object()) {
                    spdlog::warn("ClientConfig::Save: Existing {} is not a JSON object; overwriting", path);
                    userConfig = bz::json::Object();
                }
            } catch (const std::exception &ex) {
                spdlog::warn("ClientConfig::Save: Failed to parse existing {}: {}", path, ex.what());
                userConfig = bz::json::Object();
            }
        }
    }

    if (!tankPath.empty()) {
        userConfig["tankPath"] = tankPath;
    } else {
        userConfig.erase("tankPath");
    }

    bz::json::Value serverListsObject = bz::json::Object();
    serverListsObject["showLAN"] = showLanServers;
    if (!defaultServerList.empty()) {
        serverListsObject["default"] = defaultServerList;
    } else {
        serverListsObject.erase("default");
    }

    bz::json::Value communitiesArray = bz::json::Array();
    for (const auto &source : serverLists) {
        if (source.host.empty()) {
            continue;
        }

        bz::json::Value entry;
        entry["host"] = source.host;
        if (!source.name.empty()) {
            entry["name"] = source.name;
        }
        communitiesArray.push_back(std::move(entry));
    }

    serverListsObject["communities"] = std::move(communitiesArray);
    userConfig["serverLists"] = std::move(serverListsObject);

    if (auto guiIt = userConfig.find("gui"); guiIt != userConfig.end() && guiIt->is_object()) {
        auto &guiObject = *guiIt;
        if (auto serverListIt = guiObject.find("serverList"); serverListIt != guiObject.end() && serverListIt->is_object()) {
            serverListIt->erase("communityAutoRefresh");
            serverListIt->erase("lanAutoRefresh");
            if (serverListIt->empty()) {
                guiObject.erase("serverList");
            }
        }
        if (guiObject.empty()) {
            userConfig.erase("gui");
        }
    }

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
