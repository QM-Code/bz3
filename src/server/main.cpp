#include "spdlog/spdlog.h"
#include "engine/server_engine.hpp"
#include "game.hpp"
#include "plugin.hpp"
#include "server/server_discovery.hpp"
#include "server/terminal_commands.hpp"
#include "server/server_cli_options.hpp"
#include "common/data_dir_override.hpp"
#include "common/data_path_resolver.hpp"
#include "common/config_helpers.hpp"
#include <nlohmann/json.hpp>
#include <pybind11/embed.h>
#include <csignal>
#include <atomic>
#include <iostream>
#include <limits>
#include <poll.h>
#include <unistd.h>
#include <filesystem>
#include <vector>

#define MIN_FRAME_HZ (1.0f / 120.0f)

void ConfigureLogging(bool verbose) {
    if (verbose) {
        spdlog::set_level(spdlog::level::trace);
        spdlog::set_pattern("%Y-%m-%d %H:%M:%S.%e [%^%l%$] %v");
    } else {
        spdlog::set_level(spdlog::level::info);
        spdlog::set_pattern("%v");
    }
}

Game *g_game = nullptr;
ServerEngine *g_engine = nullptr;
std::atomic<bool> g_running{true};
namespace py = pybind11;

/**
 * Signal handler for graceful shutdown.
 *
 * @param signum The signal number.
 * 
 */
void signalHandler(int signum) {
    spdlog::info("Interrupt signal ({}) received. Shutting down...", signum);
    g_running = false;
}

int main(int argc, char *argv[]) {
    ConfigureLogging(false);

    // Register signal handler
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    const auto dataDirResult = bz::data::ApplyDataDirOverrideFromArgs(argc, argv, std::filesystem::path("server/config.json"));

    const std::vector<bz::data::ConfigLayerSpec> baseConfigSpecs = {
        {"common/config.json", "data/common/config.json", spdlog::level::err, true},
        {"server/config.json", "data/server/config.json", spdlog::level::err, true}
    };
    bz::data::InitializeConfigCache(baseConfigSpecs);

    ServerCLIOptions cliOptions;
    try {
        cliOptions = ParseServerCLIOptions(argc, argv);
    } catch (const std::exception &ex) {
        spdlog::error("Failed to parse server command line options: {}", ex.what());
        return 1;
    }

    if (cliOptions.verbose) {
        ConfigureLogging(true);
    }

    if (!cliOptions.worldSpecified) {
        spdlog::error("No world directory specified. Use -w <directory> or -D to load the bundled default world.");
        return 1;
    }

    std::filesystem::path worldDirPath = bz::data::Resolve(cliOptions.worldDir);

    if (!std::filesystem::is_directory(worldDirPath)) {
        spdlog::error("World directory not found: {}", worldDirPath.string());
        return 1;
    }
    const std::filesystem::path configPath = worldDirPath / "config.json";

    const std::vector<bz::data::ConfigLayerSpec> serverConfigSpecs = {
        {"common/config.json", "data/common/config.json", spdlog::level::err, true},
        {"server/config.json", "data/server/config.json", spdlog::level::err, true},
        {dataDirResult.userConfigPath, "user config", spdlog::level::debug, false},
        {configPath, "world config", spdlog::level::err, true}
    };

    bz::data::InitializeConfigCache(serverConfigSpecs);

    const auto *worldConfigPtr = bz::data::ConfigLayerByLabel("world config");
    if (!worldConfigPtr || !worldConfigPtr->is_object()) {
        spdlog::error("main: Failed to load world config object from {}", configPath.string());
        return 1;
    }

    const auto &mergedConfig = bz::data::ConfigCacheRoot();
    if (!mergedConfig.is_object()) {
        spdlog::error("main: Merged configuration is not a JSON object");
        return 1;
    }

    uint16_t port = cliOptions.hostPort;
    if (!cliOptions.hostPortExplicit) {
        port = bz::data::ReadUInt16Config({"network.ServerPort"}, port);
    } else {
        port = cliOptions.hostPort;
    }

    std::string serverName = bz::data::ReadStringConfig("serverName", "BZ OpenGL Server");
    std::string worldName = bz::data::ReadStringConfig("worldName", worldDirPath.filename().string());

    ServerEngine engine(port);
    g_engine = &engine;
    spdlog::trace("ServerEngine initialized successfully");

    const bool shouldZipWorld = cliOptions.customWorldProvided;

    Game game(engine, serverName, worldName, *worldConfigPtr, worldDirPath.string(), shouldZipWorld);
    g_game = &game;
    spdlog::trace("Game initialized successfully");

    ServerDiscoveryBeacon discoveryBeacon(port, serverName, worldName);

    spdlog::trace("Loading plugins...");
    py::scoped_interpreter guard{};

    // Prevent Python from writing .pyc files into data/plugins/__pycache__ when plugins are loaded
    {
        py::module_ sys = py::module_::import("sys");
        sys.attr("dont_write_bytecode") = true;
    }
    PluginAPI::loadPythonPlugins(mergedConfig);
    spdlog::trace("Plugins loaded successfully");

    TimeUtils::time lastFrameTime = TimeUtils::GetCurrentTime();
    spdlog::trace("Starting main loop");

    std::string inputBuffer;
    struct pollfd pfd = { STDIN_FILENO, POLLIN, 0 };
    std::cout << "> " << std::flush;

    while (g_running) {
        TimeUtils::time currTime = TimeUtils::GetCurrentTime();  
        TimeUtils::duration deltaTime = TimeUtils::GetElapsedTime(lastFrameTime, currTime);
        
        if (deltaTime < MIN_FRAME_HZ) {
            TimeUtils::sleep(MIN_FRAME_HZ - deltaTime);
            continue;
        }

        lastFrameTime = currTime;

        // Non-blocking check for stdin input
        if (poll(&pfd, 1, 0) > 0 && (pfd.revents & POLLIN)) {
            std::string line;
            if (std::getline(std::cin, line)) {
                if (!line.empty()) {
                    std::string response = processTerminalInput(line);

                    if (!response.empty()) {
                        std::cout << response << std::endl;
                    }
                }
                std::cout << "> " << std::flush;
            }
        }

        engine.earlyUpdate(deltaTime);
        game.update(deltaTime);
        engine.lateUpdate(deltaTime);
    }

    spdlog::info("Server shutdown complete");
    return 0;
}
