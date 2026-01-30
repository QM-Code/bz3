#include "ui/config/render_settings.hpp"

#include "ui/config/ui_config.hpp"

#include <algorithm>
#include <cmath>

namespace ui {

float RenderSettings::clampBrightness(float value) {
    return std::clamp(value, kMinBrightness, kMaxBrightness);
}

void RenderSettings::loadFromConfig() {
    reset();
    setBrightness(UiConfig::GetRenderBrightness(), false);
}

bool RenderSettings::saveToConfig() const {
    return UiConfig::SetRenderBrightness(brightnessValue);
}

void RenderSettings::reset() {
    brightnessValue = UiConfig::kDefaultRenderBrightness;
    dirty = false;
}

bool RenderSettings::setBrightness(float value, bool fromUser) {
    const float clamped = clampBrightness(value);
    if (std::abs(clamped - brightnessValue) < 0.0001f) {
        return false;
    }
    brightnessValue = clamped;
    if (fromUser) {
        dirty = true;
    }
    return true;
}

float RenderSettings::brightness() const {
    return brightnessValue;
}

bool RenderSettings::consumeDirty() {
    const bool wasDirty = dirty;
    dirty = false;
    return wasDirty;
}

void RenderSettings::clearDirty() {
    dirty = false;
}

bool RenderSettings::eraseFromConfig() {
    return UiConfig::EraseRenderBrightness();
}

} // namespace ui
