#include "server/terminal_commands.hpp"

#include "server/game.hpp"
#include "plugin.hpp"

#include <atomic>
#include <sstream>
#include <vector>

extern Game *g_game;
extern std::atomic<bool> g_running;

namespace {
std::vector<std::string> splitString(const std::string &s) {
    std::istringstream iss(s);
    std::vector<std::string> tokens;
    std::string token;
    while (iss >> token) {
        tokens.push_back(token);
    }
    return tokens;
}
}

std::string processTerminalInput(const std::string &input) {
    std::vector<std::string> args = splitString(input);
    if (args.empty()) {
        return "";
    }

    std::string cmd = args[0];

    if (cmd == "quit" || cmd == "exit") {
        g_running = false;
        return "Shutting down server...";
    }

    if (cmd == "listPlugins") {
        std::string response = "Loaded Plugins:";
        for (const auto &plugin : PluginAPI::getLoadedPluginScripts()) {
            response += "\n - " + plugin;
        }
        return response;
    }

    if (cmd == "config" || cmd == "manifest") {
        try {
            return g_game->world->config().dump(4);
        } catch (const std::exception &e) {
            return std::string("Error retrieving config: ") + e.what();
        }
    }

    if (cmd == "getAssetPath") {
        if (args.size() < 2) {
            return "Usage: getAssetPath <assetName>";
        }
        try {
            return g_game->world->resolveAssetPath(args[1]).string();
        } catch (const std::exception &e) {
            return std::string("Error: ") + e.what();
        }
    }

    if (cmd == "defaultPlayerParameters") {
        std::string response = "Default Player Parameters:";
        for (const auto &[key, val] : g_game->world->defaultPlayerParameters()) {
            response += "\n - " + key + ": " + std::to_string(val);
        }
        return response;
    }

    if (cmd == "listPlayers") {
        std::string response = "Connected Players:";
        for (const auto &client : g_game->getClients()) {
            response += "\n - ID: " + std::to_string(client->getId()) +
                        ", Name: " + client->getName() +
                        ", IP: " + client->getIP();
        }
        return response;
    }

    return std::string("Unknown command: ") + input;
}
