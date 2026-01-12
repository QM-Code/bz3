#pragma once

#include <cstdint>
#include <string>

struct ClientCLIOptions {
    std::string playerName;
    std::string connectAddr;
    uint16_t connectPort;
    std::string worldDir;
    std::string dataDir;
    std::string userConfigPath;
    bool addrExplicit = false;
    bool worldExplicit = false;
    bool dataDirExplicit = false;
    bool userConfigExplicit = false;
    bool verbose = false;
};

ClientCLIOptions ParseClientCLIOptions(int argc, char *argv[]);
