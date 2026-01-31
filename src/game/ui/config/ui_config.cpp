#include "ui/config/ui_config.hpp"

#include "karma/common/config_store.hpp"

#include <algorithm>
#include <cctype>
#include <string>

namespace ui {
namespace {

std::string toLower(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return text;
}

float readFloatValue(const karma::json::Value &value, float fallback) {
    if (value.is_number_float()) {
        return static_cast<float>(value.get<double>());
    }
    if (value.is_number_integer()) {
        return static_cast<float>(value.get<long long>());
    }
    if (value.is_string()) {
        try {
            return std::stof(value.get<std::string>());
        } catch (...) {
            return fallback;
        }
    }
    return fallback;
}

float readFloat(std::string_view path, float fallback) {
    const auto *value = karma::config::ConfigStore::Get(path);
    if (!value) {
        return fallback;
    }
    return readFloatValue(*value, fallback);
}

bool readBool(std::string_view path, bool fallback) {
    const auto *value = karma::config::ConfigStore::Get(path);
    if (!value) {
        return fallback;
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
        const std::string text = toLower(value->get<std::string>());
        if (text == "true" || text == "1" || text == "yes" || text == "on") {
            return true;
        }
        if (text == "false" || text == "0" || text == "no" || text == "off") {
            return false;
        }
    }
    return fallback;
}

} // namespace

float UiConfig::GetRenderBrightness() {
    return readFloat("render.brightness", kDefaultRenderBrightness);
}

bool UiConfig::SetRenderBrightness(float value) {
    return karma::config::ConfigStore::Set("render.brightness", value);
}

bool UiConfig::EraseRenderBrightness() {
    return karma::config::ConfigStore::Erase("render.brightness");
}

std::optional<float> UiConfig::TryGetRenderScale() {
    const auto *value = karma::config::ConfigStore::Get("ui.RenderScale");
    if (!value) {
        return std::nullopt;
    }
    return readFloatValue(*value, kDefaultRenderScale);
}

bool UiConfig::SetRenderScale(float value) {
    return karma::config::ConfigStore::Set("ui.RenderScale", value);
}

bool UiConfig::EraseRenderScale() {
    return karma::config::ConfigStore::Erase("ui.RenderScale");
}

std::string UiConfig::GetLanguage() {
    if (const auto *value = karma::config::ConfigStore::Get("language")) {
        if (value->is_string()) {
            return value->get<std::string>();
        }
    }
    return {};
}

bool UiConfig::SetLanguage(const std::string &value) {
    return karma::config::ConfigStore::Set("language", value);
}

const karma::json::Value *UiConfig::GetCommunityCredentials() {
    return karma::config::ConfigStore::Get("gui.communityCredentials");
}

bool UiConfig::SetCommunityCredentials(const karma::json::Value &value) {
    return karma::config::ConfigStore::Set("gui.communityCredentials", value);
}

bool UiConfig::EraseCommunityCredentials() {
    return karma::config::ConfigStore::Erase("gui.communityCredentials");
}

std::optional<karma::json::Value> UiConfig::GetKeybindings() {
    return karma::config::ConfigStore::GetCopy("keybindings");
}

bool UiConfig::SetKeybindings(const karma::json::Value &value) {
    return karma::config::ConfigStore::Set("keybindings", value);
}

bool UiConfig::EraseKeybindings() {
    return karma::config::ConfigStore::Erase("keybindings");
}

std::optional<karma::json::Value> UiConfig::GetControllerKeybindings() {
    return karma::config::ConfigStore::GetCopy("gui.keybindings.controller");
}

bool UiConfig::SetControllerKeybindings(const karma::json::Value &value) {
    return karma::config::ConfigStore::Set("gui.keybindings.controller", value);
}

bool UiConfig::EraseControllerKeybindings() {
    return karma::config::ConfigStore::Erase("gui.keybindings.controller");
}

bool UiConfig::GetHudScoreboard() {
    return readBool("ui.hud.scoreboard", kDefaultHudScoreboard);
}

bool UiConfig::GetHudChat() {
    return readBool("ui.hud.chat", kDefaultHudChat);
}

bool UiConfig::GetHudRadar() {
    return readBool("ui.hud.radar", kDefaultHudRadar);
}

bool UiConfig::GetHudFps() {
    return readBool("ui.hud.fps", kDefaultHudFps);
}

bool UiConfig::GetHudCrosshair() {
    return readBool("ui.hud.crosshair", kDefaultHudCrosshair);
}

bool UiConfig::GetValidateUi() {
    return readBool("ui.Validate", kDefaultValidateUi);
}

bool UiConfig::SetHudScoreboard(bool value) {
    return karma::config::ConfigStore::Set("ui.hud.scoreboard", value);
}

bool UiConfig::SetHudChat(bool value) {
    return karma::config::ConfigStore::Set("ui.hud.chat", value);
}

bool UiConfig::SetHudRadar(bool value) {
    return karma::config::ConfigStore::Set("ui.hud.radar", value);
}

bool UiConfig::SetHudFps(bool value) {
    return karma::config::ConfigStore::Set("ui.hud.fps", value);
}

bool UiConfig::SetHudCrosshair(bool value) {
    return karma::config::ConfigStore::Set("ui.hud.crosshair", value);
}

} // namespace ui
