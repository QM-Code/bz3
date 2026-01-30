#include "ui/config/config.hpp"

#include "common/config_store.hpp"
#include "spdlog/spdlog.h"

#include <algorithm>
#include <cctype>
#include <string>

namespace ui::config {
namespace {

const bz::json::Value* getValue(const char* path) {
    const auto* value = bz::config::ConfigStore::Get(path);
    if (!value) {
        spdlog::error("Config '{}' is missing", path);
    }
    return value;
}

}

float GetRequiredFloat(const char* path) {
    const auto* value = getValue(path);
    if (!value) {
        return 0.0f;
    }
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
            spdlog::error("Config '{}' string value is not a valid float", path);
        }
    } else {
        spdlog::error("Config '{}' cannot be interpreted as float", path);
    }
    return 0.0f;
}

std::string GetRequiredString(const char* path) {
    const auto* value = getValue(path);
    if (!value) {
        return {};
    }
    if (value->is_string()) {
        return value->get<std::string>();
    }
    spdlog::error("Config '{}' must be a string", path);
    return {};
}

bool GetRequiredBool(const char* path) {
    const auto* value = getValue(path);
    if (!value) {
        return false;
    }
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
        spdlog::error("Config '{}' string value is not a valid boolean", path);
    } else {
        spdlog::error("Config '{}' cannot be interpreted as boolean", path);
    }
    return false;
}

} // namespace ui::config
