#include "ui/hud_settings.hpp"

namespace ui {
namespace {

struct HudDefaults {
    bool scoreboard = true;
    bool chat = true;
    bool radar = true;
    bool fps = false;
    bool crosshair = true;
};

const HudDefaults kDefaults{};

}

bool HudSettings::readBool(const bz::json::Value &node, const char *key, bool fallback) {
    if (!node.is_object()) {
        return fallback;
    }
    auto it = node.find(key);
    if (it == node.end() || !it->is_boolean()) {
        return fallback;
    }
    return it->get<bool>();
}

void HudSettings::load(const bz::json::Value &userConfig) {
    reset();
    if (!userConfig.is_object()) {
        return;
    }
    const auto uiIt = userConfig.find("ui");
    if (uiIt == userConfig.end() || !uiIt->is_object()) {
        return;
    }
    const auto hudIt = uiIt->find("hud");
    if (hudIt == uiIt->end() || !hudIt->is_object()) {
        return;
    }
    scoreboardVisibleValue = readBool(*hudIt, "scoreboard", scoreboardVisibleValue);
    chatVisibleValue = readBool(*hudIt, "chat", chatVisibleValue);
    radarVisibleValue = readBool(*hudIt, "radar", radarVisibleValue);
    fpsVisibleValue = readBool(*hudIt, "fps", fpsVisibleValue);
    crosshairVisibleValue = readBool(*hudIt, "crosshair", crosshairVisibleValue);
}

void HudSettings::save(bz::json::Value &userConfig) const {
    if (!userConfig.is_object()) {
        userConfig = bz::json::Object();
    }
    auto &uiNode = userConfig["ui"];
    if (!uiNode.is_object()) {
        uiNode = bz::json::Object();
    }
    auto &hudNode = uiNode["hud"];
    if (!hudNode.is_object()) {
        hudNode = bz::json::Object();
    }
    hudNode["scoreboard"] = scoreboardVisibleValue;
    hudNode["chat"] = chatVisibleValue;
    hudNode["radar"] = radarVisibleValue;
    hudNode["fps"] = fpsVisibleValue;
    hudNode["crosshair"] = crosshairVisibleValue;
}

void HudSettings::reset() {
    scoreboardVisibleValue = kDefaults.scoreboard;
    chatVisibleValue = kDefaults.chat;
    radarVisibleValue = kDefaults.radar;
    fpsVisibleValue = kDefaults.fps;
    crosshairVisibleValue = kDefaults.crosshair;
    dirty = false;
}

bool HudSettings::setScoreboardVisible(bool value, bool fromUser) {
    if (value == scoreboardVisibleValue) {
        return false;
    }
    scoreboardVisibleValue = value;
    if (fromUser) {
        dirty = true;
    }
    return true;
}

bool HudSettings::setChatVisible(bool value, bool fromUser) {
    if (value == chatVisibleValue) {
        return false;
    }
    chatVisibleValue = value;
    if (fromUser) {
        dirty = true;
    }
    return true;
}

bool HudSettings::setRadarVisible(bool value, bool fromUser) {
    if (value == radarVisibleValue) {
        return false;
    }
    radarVisibleValue = value;
    if (fromUser) {
        dirty = true;
    }
    return true;
}

bool HudSettings::setFpsVisible(bool value, bool fromUser) {
    if (value == fpsVisibleValue) {
        return false;
    }
    fpsVisibleValue = value;
    if (fromUser) {
        dirty = true;
    }
    return true;
}

bool HudSettings::setCrosshairVisible(bool value, bool fromUser) {
    if (value == crosshairVisibleValue) {
        return false;
    }
    crosshairVisibleValue = value;
    if (fromUser) {
        dirty = true;
    }
    return true;
}

bool HudSettings::consumeDirty() {
    const bool wasDirty = dirty;
    dirty = false;
    return wasDirty;
}

void HudSettings::clearDirty() {
    dirty = false;
}

} // namespace ui
