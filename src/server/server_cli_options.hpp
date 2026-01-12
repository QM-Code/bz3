#pragma once

#include <cstdint>
#include <string>

struct ServerCLIOptions {
    std::string worldDir;
    bool worldSpecified = false;
    bool customWorldProvided = false;
    uint16_t hostPort;
    bool hostPortExplicit = false;
    std::string dataDir;
    std::string userConfigPath;
    bool dataDirExplicit = false;
    bool userConfigExplicit = false;
    bool verbose = false;
};

ServerCLIOptions ParseServerCLIOptions(int argc, char *argv[]);
