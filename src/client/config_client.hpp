#pragma once

#include <string>
#include <vector>

struct ClientServerListSource {
    std::string name;
    std::string url;
};

struct ClientConfig {
    std::string tankPath;
    std::vector<ClientServerListSource> serverLists;
    bool showLanServers = false;
    std::string defaultServerList;

    static ClientConfig Load(const std::string &path);
    bool Save(const std::string &path) const;
};
