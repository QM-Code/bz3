#include "server/server_cli_options.hpp"

#include "common/data_path_resolver.hpp"
#include "cxxopts.hpp"
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <cstdlib>
#include <iostream>

namespace {

std::string ConfiguredPortDefault() {
    if (const auto *portNode = bz::data::ConfigValue("network.ServerPort")) {
        if (portNode->is_string()) {
            return portNode->get<std::string>();
        }
        if (portNode->is_number_unsigned()) {
            return std::to_string(portNode->get<unsigned int>());
        }
    }
    return std::string("0");
}

} // namespace

ServerCLIOptions ParseServerCLIOptions(int argc, char *argv[]) {
    cxxopts::Options options("bz3-server", "BZ3 server");
    options.add_options()
        ("w,world", "World directory", cxxopts::value<std::string>())
        ("D,default-world", "Use bundled default world")
        ("p,port", "Server listen port", cxxopts::value<uint16_t>()->default_value(ConfiguredPortDefault()))
        ("d,data-dir", "Data directory (overrides BZ3_DATA_DIR)", cxxopts::value<std::string>())
        ("c,config", "User config file path", cxxopts::value<std::string>())
        ("v,verbose", "Enable verbose logging")
        ("h,help", "Show help");

    cxxopts::ParseResult result;
    try {
        result = options.parse(argc, argv);
    } catch (const cxxopts::exceptions::exception &ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        std::cerr << options.help() << std::endl;
        std::exit(1);
    }

    if (result.count("help")) {
        std::cout << options.help() << std::endl;
        std::exit(0);
    }

    ServerCLIOptions parsed;
    if (result.count("world") && result.count("default-world")) {
        throw std::runtime_error("Cannot specify both -w/--world and -D/--default-world");
    }

    if (result.count("default-world")) {
        parsed.worldSpecified = true;

        const auto serverConfigPath = bz::data::Resolve("server/config.json");
        auto serverConfigOpt = bz::data::LoadJsonFile(serverConfigPath, "data/server/config.json", spdlog::level::err);
        if (!serverConfigOpt || !serverConfigOpt->is_object()) {
            throw std::runtime_error("default world flag requires data/server/config.json to be a JSON object");
        }

        auto it = serverConfigOpt->find("defaultWorld");
        if (it == serverConfigOpt->end() || !it->is_string()) {
            throw std::runtime_error("defaultWorld missing or not a string in data/server/config.json");
        }

        parsed.worldDir = it->get<std::string>();
        parsed.customWorldProvided = false;
    }

    if (result.count("world")) {
        parsed.worldDir = result["world"].as<std::string>();
        parsed.worldSpecified = true;
        parsed.customWorldProvided = true;
    }

    parsed.dataDir = result.count("data-dir") ? result["data-dir"].as<std::string>() : std::string();
    parsed.userConfigPath = result.count("config") ? result["config"].as<std::string>() : std::string();
    parsed.hostPort = result["port"].as<uint16_t>();
    parsed.hostPortExplicit = result.count("port") > 0;
    parsed.dataDirExplicit = result.count("data-dir") > 0;
    parsed.userConfigExplicit = result.count("config") > 0;
    parsed.verbose = result.count("verbose") > 0;
    return parsed;
}
