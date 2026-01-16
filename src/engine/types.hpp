#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/glm.hpp>
#include <type_traits>
#include <cstddef>
#include <string>
#include <vector>
#include <chrono>
#include <unordered_map>
#include <thread>
#include <map>

typedef struct InputState {
    bool fire;
    bool spawn;
    bool jump;
    bool quickQuit;
    bool chat;
    bool escape;
    bool toggleFullscreen;
    glm::vec2 movement;
} InputState;

using SettingsMap = std::unordered_map<std::string, float>;
using ConfigMap = std::unordered_map<std::string, float>;

namespace TimeUtils {
    using time = std::chrono::time_point<std::chrono::system_clock>;
    using duration = float;

    inline duration GetElapsedTime(time start, time end) {
        return std::chrono::duration<float>(end - start).count();
    }

    inline time GetCurrentTime() {
        return std::chrono::system_clock::now();
    }

    inline duration getDuration(float seconds) {
        return seconds;
    }

    inline void sleep(duration seconds) {
        std::this_thread::sleep_for(std::chrono::duration<float>(seconds));
    }
}

typedef struct Location {
    glm::vec3 position;
    glm::quat rotation;
} Location;

inline float angleBetween(const glm::quat& a, const glm::quat& b, bool degrees = true) {
    glm::quat qa = glm::normalize(a);
    glm::quat qb = glm::normalize(b);

    float d = glm::dot(qa, qb);
    d = glm::clamp(glm::abs(d), -1.0f, 1.0f);

    float ret = 2.0f * std::acos(d); // radians
    if (degrees) {
        ret = glm::degrees(ret);
    }
    return ret;
}

#define SERVER_CLIENT_ID 0
#define BROADCAST_CLIENT_ID 1
#define FIRST_CLIENT_ID 2

using client_id = uint32_t;
using shot_id = uint32_t;

using render_id = uint32_t;

using PlayerParameters = std::map<std::string, float>;

constexpr uint32_t NET_PROTOCOL_VERSION = 1;

typedef struct PlayerState {
    std::string name;
    glm::vec3 position;
    glm::quat rotation;
    glm::vec3 velocity;
    bool alive;
    int score = 0;
    PlayerParameters params;
} PlayerState;


/*
 * Server messages
 */

enum ServerMsg_Type {
    ServerMsg_Type_PLAYER_JOIN,
    ServerMsg_Type_PLAYER_LEAVE,
    ServerMsg_Type_PLAYER_STATE,
    ServerMsg_Type_PLAYER_PARAMETERS,
    ServerMsg_Type_PLAYER_LOCATION,
    ServerMsg_Type_PLAYER_SPAWN,
    ServerMsg_Type_PLAYER_DEATH,
    ServerMsg_Type_SET_SCORE,
    ServerMsg_Type_CREATE_SHOT,
    ServerMsg_Type_REMOVE_SHOT,
    ServerMsg_Type_INIT,
    ServerMsg_Type_CHAT
};

typedef struct ServerMsg {
    ServerMsg_Type type;
} ServerMsg;

typedef struct ServerMsg_PlayerJoin : ServerMsg {
    static constexpr ServerMsg_Type Type = ServerMsg_Type_PLAYER_JOIN;
    ServerMsg_PlayerJoin() { type = Type; }
    client_id clientId;
    PlayerState state;
} ServerMsg_PlayerJoin;

typedef struct ServerMsg_PlayerLeave : ServerMsg {
    static constexpr ServerMsg_Type Type = ServerMsg_Type_PLAYER_LEAVE;
    ServerMsg_PlayerLeave() { type = Type; }
    client_id clientId;
} ServerMsg_PlayerLeave;

typedef struct ServerMsg_PlayerState : ServerMsg {
    static constexpr ServerMsg_Type Type = ServerMsg_Type_PLAYER_STATE;
    ServerMsg_PlayerState() { type = Type; }
    client_id clientId;
    PlayerState state;
} ServerMsg_PlayerState;

typedef struct ServerMsg_PlayerParameters : ServerMsg {
    static constexpr ServerMsg_Type Type = ServerMsg_Type_PLAYER_PARAMETERS;
    ServerMsg_PlayerParameters() { type = Type; }
    client_id clientId;
    PlayerParameters params;
} ServerMsg_PlayerParameters;

typedef struct ServerMsg_PlayerLocation : ServerMsg {
    static constexpr ServerMsg_Type Type = ServerMsg_Type_PLAYER_LOCATION;
    ServerMsg_PlayerLocation() { type = Type; }
    client_id clientId;
    glm::vec3 position;
    glm::quat rotation;
    glm::vec3 velocity;
} ServerMsg_PlayerLocation;

typedef struct ServerMsg_PlayerSpawn : ServerMsg {
    static constexpr ServerMsg_Type Type = ServerMsg_Type_PLAYER_SPAWN;
    ServerMsg_PlayerSpawn() { type = Type; }
    client_id clientId;
    glm::vec3 position;
    glm::quat rotation;
    glm::vec3 velocity;
} ServerMsg_PlayerSpawn;

typedef struct ServerMsg_PlayerDeath : ServerMsg {
    static constexpr ServerMsg_Type Type = ServerMsg_Type_PLAYER_DEATH;
    ServerMsg_PlayerDeath() { type = Type; }
    client_id clientId;
} ServerMsg_PlayerDeath;

typedef struct ServerMsg_SetScore : ServerMsg {
    static constexpr ServerMsg_Type Type = ServerMsg_Type_SET_SCORE;
    ServerMsg_SetScore() { type = Type; }
    client_id clientId;
    int32_t score;
} ServerMsg_SetScore;
    
typedef struct ServerMsg_CreateShot : ServerMsg {
    static constexpr ServerMsg_Type Type = ServerMsg_Type_CREATE_SHOT;
    ServerMsg_CreateShot() { type = Type; }
    shot_id globalShotId;
    glm::vec3 position;
    glm::vec3 velocity;
} ServerMsg_CreateShot;

typedef struct ServerMsg_RemoveShot : ServerMsg {
    static constexpr ServerMsg_Type Type = ServerMsg_Type_REMOVE_SHOT;
    ServerMsg_RemoveShot() { type = Type; }
    shot_id shotId;
    bool isGlobalId;
} ServerMsg_RemoveShot;

typedef struct ServerMsg_Chat : ServerMsg {
    static constexpr ServerMsg_Type Type = ServerMsg_Type_CHAT;
    ServerMsg_Chat() { type = Type; }
    client_id fromId;
    client_id toId;
    std::string text;
} ServerMsg_Chat;

typedef struct ServerMsg_Init : ServerMsg {
    static constexpr ServerMsg_Type Type = ServerMsg_Type_INIT;
    ServerMsg_Init() { type = Type; }
    client_id clientId;
    std::string serverName;
    std::string worldName;
    uint32_t protocolVersion = NET_PROTOCOL_VERSION;
    std::vector<std::string> features;
    PlayerParameters defaultPlayerParams;
    std::vector<std::byte> worldData;
} ServerMsg_Init;

/*
 * Client messages
 */

enum ClientMsg_Type {
    ClientMsg_Type_PLAYER_JOIN,
    ClientMsg_Type_PLAYER_LEAVE,
    ClientMsg_Type_REQUEST_PLAYER_SPAWN,
    ClientMsg_Type_PLAYER_LOCATION,
    ClientMsg_Type_CREATE_SHOT,
    ClientMsg_Type_CHAT
};

typedef struct ClientMsg {
    ClientMsg_Type type;
    client_id clientId;
} ClientMsg;

typedef struct ClientMsg_PlayerJoin : ClientMsg {
    static constexpr ClientMsg_Type Type = ClientMsg_Type_PLAYER_JOIN;
    ClientMsg_PlayerJoin() { type = Type; }
    std::string ip;
    std::string name;
    uint32_t protocolVersion = NET_PROTOCOL_VERSION;
} ClientMsg_PlayerJoin;

typedef struct ClientMsg_PlayerLeave : ClientMsg {
    static constexpr ClientMsg_Type Type = ClientMsg_Type_PLAYER_LEAVE;
    ClientMsg_PlayerLeave() { type = Type; }
} ClientMsg_PlayerLeave;

typedef struct ClientMsg_RequestPlayerSpawn : ClientMsg {
    static constexpr ClientMsg_Type Type = ClientMsg_Type_REQUEST_PLAYER_SPAWN;
    ClientMsg_RequestPlayerSpawn() { type = Type; }
} ClientMsg_RequestPlayerSpawn;

typedef struct ClientMsg_PlayerLocation : ClientMsg {
    static constexpr ClientMsg_Type Type = ClientMsg_Type_PLAYER_LOCATION;
    ClientMsg_PlayerLocation() { type = Type; }
    glm::vec3 position;
    glm::quat rotation;
} ClientMsg_PlayerLocation;

typedef struct ClientMsg_CreateShot : ClientMsg {
    static constexpr ClientMsg_Type Type = ClientMsg_Type_CREATE_SHOT;
    ClientMsg_CreateShot() { type = Type; }
    shot_id localShotId;
    glm::vec3 position;
    glm::vec3 velocity;
} ClientMsg_CreateShot;

typedef struct ClientMsg_Chat : ClientMsg {
    static constexpr ClientMsg_Type Type = ClientMsg_Type_CHAT;
    ClientMsg_Chat() { type = Type; }
    client_id toId;
    std::string text;
} ClientMsg_Chat;
