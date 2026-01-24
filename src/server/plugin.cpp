#include "plugin.hpp"
#include "game.hpp"
#include "engine/server_engine.hpp"
#include "common/data_path_resolver.hpp"
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/embed.h>
#include <filesystem>



extern Game* g_game;
extern ServerEngine* g_engine;
std::map<EventType, std::vector<pybind11::function>> g_pluginCallbacks;
namespace {
std::vector<std::string> g_loadedPlugins;
}

void PluginAPI::loadPythonPlugins(const nlohmann::json &configJson) {
    namespace py = pybind11;
    namespace fs = std::filesystem;

    std::vector<std::string> configuredPlugins;
    if (configJson.contains("plugins") && configJson["plugins"].is_array()) {
        for (const auto &entry : configJson["plugins"]) {
            if (!entry.is_object()) {
                spdlog::warn("Skipping plugin entry because it is not an object.");
                continue;
            }

            auto nameIt = entry.find("name");
            if (nameIt != entry.end() && nameIt->is_string()) {
                configuredPlugins.emplace_back(nameIt->get<std::string>());
            } else {
                spdlog::warn("Skipping plugin entry missing a string 'name' field.");
            }
        }
    }

    if (configuredPlugins.empty()) {
        spdlog::info("No plugins configured in world config; skipping Python plugin load.");
        g_loadedPlugins.clear();
        return;
    }

    const fs::path dataRoot = bz::data::DataRoot();
    const fs::path pluginDir = dataRoot / "plugins";
    const fs::path commandsDir = pluginDir / "commands";
    const fs::path sharedPythonDir = dataRoot / "python";

    if (!fs::exists(pluginDir)) {
        spdlog::error("Plugin directory not found at {}", pluginDir.string());
        g_loadedPlugins.clear();
        return;
    }

    py::module_ sys  = py::module_::import("sys");

    auto addSysPath = [&](const fs::path &path) {
        if (!path.empty() && fs::exists(path)) {
            sys.attr("path").attr("insert")(0, path.lexically_normal().string());
        }
    };

    addSysPath(dataRoot);
    addSysPath(pluginDir);
    addSysPath(sharedPythonDir);
    if (fs::exists(commandsDir)) {
        addSysPath(commandsDir);
    }

    g_loadedPlugins.clear();

    auto isPluginNameSafe = [](const std::string &pluginName) {
        return !pluginName.empty() &&
               pluginName.find("..") == std::string::npos &&
               pluginName.find('/') == std::string::npos &&
               pluginName.find('\\') == std::string::npos;
    };

    for (const auto &pluginName : configuredPlugins) {
        if (!isPluginNameSafe(pluginName)) {
            spdlog::warn("Skipping plugin '{}' because it contains invalid path characters.", pluginName);
            continue;
        }

        const fs::path scriptPath = pluginDir / pluginName / "plugin.py";
        if (!fs::exists(scriptPath)) {
            spdlog::warn("Configured plugin '{}' missing at {}", pluginName, scriptPath.string());
            continue;
        }

        addSysPath(scriptPath.parent_path());

        try {
            const std::string normalizedPath = scriptPath.lexically_normal().string();
            py::print("[PY] Loading plugin:", pluginName, "->", normalizedPath);
            py::eval_file(normalizedPath, py::globals());
            g_loadedPlugins.push_back(normalizedPath);
        } catch (py::error_already_set &e) {
            py::print("[PY ERROR]", e.what());
        }
    }
}

const std::vector<std::string> &PluginAPI::getLoadedPluginScripts() {
    return g_loadedPlugins;
}

void PluginAPI::registerCallback(EventType type, pybind11::function func) {
    if (g_pluginCallbacks.find(type) == g_pluginCallbacks.end()) {
        g_pluginCallbacks[type] = std::vector<pybind11::function>();
    }
    g_pluginCallbacks[type].push_back(func);
    spdlog::debug("PluginAPI: Registered callback for event {} (total: {})", static_cast<int>(type), g_pluginCallbacks[type].size());
}

void PluginAPI::sendChatMessage(client_id fromId, client_id toId, const std::string &text) {
    ServerMsg_Chat serverChatMsg;
    serverChatMsg.fromId = fromId;
    serverChatMsg.toId = toId;
    serverChatMsg.text = text;
    g_engine->network->send<ServerMsg_Chat>(toId, &serverChatMsg);
}

bool PluginAPI::setPlayerParameter(client_id playerId, const std::string &param, const pybind11::object &value) {
    Client* client = g_game->getClient(playerId);
    if (client) {
        return client->setParameter(param, value.cast<float>());
    }
    return false;
}

void PluginAPI::killPlayer(client_id targetId) {
    Client *client = g_game->getClient(targetId);

    if (client) {
        client->setScore(client->getScore() - 1);
        client->die();
    }
}

void PluginAPI::disconnectPlayer(client_id targetId, const std::string &reason) {
    if (!g_engine || !g_engine->network) {
        spdlog::warn("PluginAPI::disconnectPlayer: Server engine not initialized");
        return;
    }

    Client *client = g_game ? g_game->getClient(targetId) : nullptr;
    if (!client) {
        spdlog::warn("PluginAPI::disconnectPlayer: Client id {} not found", targetId);
        return;
    }

    g_engine->network->disconnectClient(targetId, reason);
}

client_id PluginAPI::getPlayerByName(const std::string &name) {
    Client* client = g_game->getClientByName(name);
    if (client) {
        return client->getId();
    }
    return 0;
}

std::vector<client_id> PluginAPI::getAllPlayerIds() {
    std::vector<client_id> ids;
    for (const auto &client : g_game->getClients()) {
        ids.push_back(client->getId());
    }
    return ids;
}

std::optional<std::string> PluginAPI::getPlayerName(client_id id) {
    Client* client = g_game->getClient(id);
    if (client) {
        return client->getName();
    }
    return std::nullopt;
}

std::optional<std::string> PluginAPI::getPlayerIP(client_id id) {
    Client* client = g_game->getClient(id);
    if (client) {
        return client->getIP();
    }
    return std::nullopt;
}

PYBIND11_EMBEDDED_MODULE(bzapi, m) {
    m.doc() = "Plugin API for BZ OpenGL server plugins";

    // Register the enum
    pybind11::enum_<EventType>(m, "EventType")
        .value("PLAYER_JOIN", EventType_PlayerJoin)
        .value("PLAYER_LEAVE", EventType_PlayerLeave)
        .value("PLAYER_SPAWN", EventType_PlayerSpawn)
        .value("PLAYER_DIE", EventType_PlayerDie)
        .value("CREATE_SHOT", EventType_CreateShot)
        .value("CHAT", EventType_Chat)
        .export_values();

    // Callback registration function
    m.def("register_callback", &PluginAPI::registerCallback, "Register a callback",
          pybind11::arg("type"), pybind11::arg("callback"));

    m.def("send_chat_message", &PluginAPI::sendChatMessage, "Send a chat message",
          pybind11::arg("from_id"), pybind11::arg("to_id"), pybind11::arg("text"));
    m.def("set_player_parameter", &PluginAPI::setPlayerParameter, "Set a player parameter",
          pybind11::arg("player_id"), pybind11::arg("param"), pybind11::arg("value"));
    m.def("kill_player", &PluginAPI::killPlayer, "Kill a player",
          pybind11::arg("target_id"));
        m.def("disconnect_player", &PluginAPI::disconnectPlayer, "Disconnect a player",
            pybind11::arg("target_id"), pybind11::arg("reason") = "");
        m.def("kick_player", &PluginAPI::disconnectPlayer, "Disconnect a player",
            pybind11::arg("target_id"), pybind11::arg("reason") = "");
    m.def("get_player_by_name", &PluginAPI::getPlayerByName, "Get a player ID by name",
          pybind11::arg("name"));
    m.def("get_all_player_ids", &PluginAPI::getAllPlayerIds, "Get all player IDs");
    m.def("get_player_name", &PluginAPI::getPlayerName, "Get a player's name by ID",
          pybind11::arg("id"));
    m.def("get_player_ip", &PluginAPI::getPlayerIP, "Get a player's IP by ID",
          pybind11::arg("id"));
}
