#pragma once

#include "common/json.hpp"

namespace ui {

class HudSettings {
public:
    void load(const bz::json::Value &userConfig);
    void save(bz::json::Value &userConfig) const;
    void reset();

    bool scoreboardVisible() const { return scoreboardVisibleValue; }
    bool chatVisible() const { return chatVisibleValue; }
    bool radarVisible() const { return radarVisibleValue; }
    bool fpsVisible() const { return fpsVisibleValue; }
    bool crosshairVisible() const { return crosshairVisibleValue; }

    bool setScoreboardVisible(bool value, bool fromUser);
    bool setChatVisible(bool value, bool fromUser);
    bool setRadarVisible(bool value, bool fromUser);
    bool setFpsVisible(bool value, bool fromUser);
    bool setCrosshairVisible(bool value, bool fromUser);

    bool consumeDirty();
    void clearDirty();

private:
    static bool readBool(const bz::json::Value &node, const char *key, bool fallback);

    bool scoreboardVisibleValue = true;
    bool chatVisibleValue = true;
    bool radarVisibleValue = true;
    bool fpsVisibleValue = false;
    bool crosshairVisibleValue = true;
    bool dirty = false;
};

} // namespace ui
