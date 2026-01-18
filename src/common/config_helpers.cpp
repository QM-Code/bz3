#include "common/config_helpers.hpp"

#include "common/data_path_resolver.hpp"
#include "spdlog/spdlog.h"

#include <algorithm>
#include <cctype>
#include <limits>

namespace bz::data {

bool ReadBoolConfig(std::initializer_list<const char*> paths, bool defaultValue) {
    for (const char* path : paths) {
        if (const auto* value = ConfigValue(path)) {
            if (value->is_boolean()) {
                return value->get<bool>();
            }
            if (value->is_number_integer()) {
                return value->get<long long>() != 0;
            }
            if (value->is_number_float()) {
                return value->get<double>() != 0.0;
            }
            if (value->is_string()) {
                std::string text = value->get<std::string>();
                std::transform(text.begin(), text.end(), text.begin(),
                               [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
                if (text == "true" || text == "1" || text == "yes" || text == "on") {
                    return true;
                }
                if (text == "false" || text == "0" || text == "no" || text == "off") {
                    return false;
                }
            }
            spdlog::warn("Config '{}' cannot be interpreted as boolean", path);
        }
    }
    return defaultValue;
}

uint16_t ReadUInt16Config(std::initializer_list<const char*> paths, uint16_t defaultValue) {
    for (const char* path : paths) {
        if (auto value = ConfigValueUInt16(path)) {
            if (*value > 0) {
                return *value;
            }
            spdlog::warn("Config '{}' must be positive; falling back", path);
            return defaultValue;
        }

        if (const auto* raw = ConfigValue(path); raw && raw->is_string()) {
            try {
                const auto parsed = std::stoul(raw->get<std::string>());
                if (parsed > 0 && parsed <= std::numeric_limits<uint16_t>::max()) {
                    return static_cast<uint16_t>(parsed);
                }
            } catch (...) {
                spdlog::warn("Config '{}' string value is not a valid uint16", path);
            }
            return defaultValue;
        }
    }
    return defaultValue;
}

float ReadFloatConfig(std::initializer_list<const char*> paths, float defaultValue) {
    for (const char* path : paths) {
        if (const auto* value = ConfigValue(path)) {
            if (value->is_number_float()) {
                return static_cast<float>(value->get<double>());
            }
            if (value->is_number_integer()) {
                return static_cast<float>(value->get<long long>());
            }
            if (value->is_string()) {
                try {
                    return std::stof(value->get<std::string>());
                } catch (...) {
                    spdlog::warn("Config '{}' string value is not a valid float", path);
                }
            } else {
                spdlog::warn("Config '{}' cannot be interpreted as float", path);
            }
        }
    }
    return defaultValue;
}

std::string ReadStringConfig(const char *path, const std::string &defaultValue) {
    if (const auto* value = ConfigValue(path)) {
        if (value->is_string()) {
            return value->get<std::string>();
        }
    }
    return defaultValue;
}

} // namespace bz::data
