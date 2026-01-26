#pragma once

#include "common/json.hpp"

namespace ui {

class RenderSettings {
public:
    static constexpr float kMinBrightness = 0.2f;
    static constexpr float kMaxBrightness = 3.0f;

    void load(const bz::json::Value &userConfig);
    void save(bz::json::Value &userConfig) const;
    void reset();
    bool setBrightness(float value, bool fromUser);
    float brightness() const;
    bool consumeDirty();
    void clearDirty();
    static void eraseFromConfig(bz::json::Value &userConfig);

private:
    static float clampBrightness(float value);

    float brightnessValue = 1.0f;
    bool dirty = false;
};

} // namespace ui
