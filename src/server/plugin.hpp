#pragma once
#include "engine/types.hpp"
#include "spdlog/spdlog.h"
#include <nlohmann/json.hpp>
#include <vector>
#include <memory>
#include <map>
#include <string>
#include <pybind11/pybind11.h>

namespace py = pybind11;
extern std::map<ClientMsg_Type, std::vector<pybind11::function>> g_pluginCallbacks;

template<typename T> inline bool g_runPluginCallbacks(T &msg) {
    // Make sure that T of type ClientMsg or substruct (static assert)
    static_assert(std::is_base_of<ClientMsg, T>::value, "T must be derived from ClientMsg");

    py::gil_scoped_acquire gil;
    bool ret = false;

    if (g_pluginCallbacks.find(T::Type) != g_pluginCallbacks.end()) {
        for (auto& callback : g_pluginCallbacks[T::Type]) {
            try {
                if (T::Type == ClientMsg_Type::ClientMsg_Type_CHAT) {
                    auto& chatMsg = static_cast<ClientMsg_Chat&>(msg);
                    bool handled = callback(chatMsg.clientId, chatMsg.toId, std::string(chatMsg.text)).template cast<bool>();
                    if (handled) {
                        ret = true;
                    }
                } else {    
                    spdlog::warn("Plugin callback for message type {} not implemented", static_cast<int>(T::Type));
                }
            } catch (const std::exception &e) {
                spdlog::error("Exception in plugin callback: {}", e.what());
            }
        }
    }

    return ret;
}

namespace PluginAPI {
    void registerCallback(ClientMsg_Type type, pybind11::function func);
    void loadPythonPlugins(const nlohmann::json &configJson);
    const std::vector<std::string>& getLoadedPluginScripts();
    
    void sendChatMessage(client_id fromId, client_id toId, const std::string &text);
    bool setPlayerParameter(client_id playerId, const std::string &param, const pybind11::object &value);
    void killPlayer(client_id targetId);
    void disconnectPlayer(client_id targetId, const std::string &reason);
    client_id getPlayerByName(const std::string &name);
    std::vector<client_id> getAllPlayerIds();

    std::optional<std::string> getPlayerName(client_id id);
    std::optional<std::string> getPlayerIP(client_id id);
}
