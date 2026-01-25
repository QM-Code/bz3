#pragma once
#include "core/types.hpp"
#include "game/net/messages.hpp"
#include "spdlog/spdlog.h"
#include "common/json.hpp"
#include <vector>
#include <memory>
#include <map>
#include <string>
#include <type_traits>
#include <pybind11/pybind11.h>

enum EventType {
    EventType_Chat,
    EventType_PlayerJoin,
    EventType_PlayerLeave,
    EventType_PlayerSpawn,
    EventType_PlayerDie,
    EventType_CreateShot
};

extern std::map<EventType, std::vector<pybind11::function>> g_pluginCallbacks;

struct Event_Chat {
    client_id fromId;
    client_id toId;
    std::string message;
};

struct Event_CreateShot {
    shot_id shotId;
};

struct Event_PlayerJoin {
    std::string playerName;
    std::string ip;
};

struct Event_PlayerLeave {
    client_id playerId;
};

struct Event_PlayerSpawn {
    client_id playerId;
};

struct Event_PlayerDie {
    client_id victimPlayerId;
    shot_id shotId;
};


template<typename T> inline bool g_triggerPluginEvent(EventType type, T& eventData) {
    namespace py = pybind11;
    auto it = g_pluginCallbacks.find(type);
    bool handled = false;    

    if (it != g_pluginCallbacks.end()) {
        for (const auto &func : it->second) {
            try {
                // Get return value to check if the event was handled
                bool h = false;

                if constexpr (std::is_same_v<T, Event_Chat>) {
                    if (eventData.message.rfind("/", 0) == 0) {
                        spdlog::debug("PluginAPI: Chat command candidate '{}' (callbacks: {})", eventData.message, it->second.size());
                    }
                    if (type == EventType_Chat) {
                        h = func(eventData.fromId, eventData.toId, eventData.message).template cast<bool>();
                    }
                } else if constexpr (std::is_same_v<T, Event_PlayerJoin>) {
                    if (type == EventType_PlayerJoin) {
                        h = func(eventData.playerName, eventData.ip).template cast<bool>();
                    }
                } else if constexpr (std::is_same_v<T, Event_PlayerLeave>) {
                    if (type == EventType_PlayerLeave) {
                        h = func(eventData.playerId).template cast<bool>();
                    }
                } else if constexpr (std::is_same_v<T, Event_PlayerSpawn>) {
                    if (type == EventType_PlayerSpawn) {
                        h = func(eventData.playerId).template cast<bool>();
                    }
                } else if constexpr (std::is_same_v<T, Event_PlayerDie>) {
                    if (type == EventType_PlayerDie) {
                        h = func(eventData.victimPlayerId, eventData.shotId).template cast<bool>();
                    }
                } else if constexpr (std::is_same_v<T, Event_CreateShot>) {
                    if (type == EventType_CreateShot) {
                        h = func(eventData.shotId).template cast<bool>();
                    }
                } else {
                    static_assert(!std::is_same_v<T, T>, "Unsupported plugin event type");
                }

                if (h) {
                    handled = true;
                }
            } catch (const py::error_already_set &e) {
                spdlog::error("Error in plugin callback for event type {}: {}", static_cast<int>(type), e.what());
            }
        }
    }

    return handled;
}

namespace py = pybind11;

namespace PluginAPI {
    void registerCallback(EventType type, pybind11::function func);
    void loadPythonPlugins(const bz::json::Value &configJson);
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
