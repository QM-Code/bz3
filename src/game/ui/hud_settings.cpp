#include "ui/hud_settings.hpp"

#include "ui/ui_config.hpp"

namespace ui {

void HudSettings::loadFromConfig() {
    reset();
    scoreboardVisibleValue = UiConfig::GetHudScoreboard();
    chatVisibleValue = UiConfig::GetHudChat();
    radarVisibleValue = UiConfig::GetHudRadar();
    fpsVisibleValue = UiConfig::GetHudFps();
    crosshairVisibleValue = UiConfig::GetHudCrosshair();
}

bool HudSettings::saveToConfig() const {
    return UiConfig::SetHudScoreboard(scoreboardVisibleValue) &&
        UiConfig::SetHudChat(chatVisibleValue) &&
        UiConfig::SetHudRadar(radarVisibleValue) &&
        UiConfig::SetHudFps(fpsVisibleValue) &&
        UiConfig::SetHudCrosshair(crosshairVisibleValue);
}

void HudSettings::reset() {
    scoreboardVisibleValue = UiConfig::kDefaultHudScoreboard;
    chatVisibleValue = UiConfig::kDefaultHudChat;
    radarVisibleValue = UiConfig::kDefaultHudRadar;
    fpsVisibleValue = UiConfig::kDefaultHudFps;
    crosshairVisibleValue = UiConfig::kDefaultHudCrosshair;
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
