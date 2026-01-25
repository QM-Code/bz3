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

using render_id = uint32_t;

using PlayerParameters = std::map<std::string, float>;
