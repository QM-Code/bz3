#include "ui/render_settings.hpp"

#include <algorithm>
#include <cmath>

namespace ui {

float RenderSettings::clampBrightness(float value) {
    return std::clamp(value, kMinBrightness, kMaxBrightness);
}

void RenderSettings::load(const bz::json::Value &userConfig) {
    brightnessValue = 1.0f;
    if (!userConfig.is_object()) {
        return;
    }
    if (auto renderIt = userConfig.find("render"); renderIt != userConfig.end() && renderIt->is_object()) {
        if (auto brightIt = renderIt->find("brightness"); brightIt != renderIt->end() && brightIt->is_number()) {
            brightnessValue = clampBrightness(static_cast<float>(brightIt->get<double>()));
        }
    }
}

void RenderSettings::save(bz::json::Value &userConfig) const {
    if (!userConfig.is_object()) {
        userConfig = bz::json::Object();
    }
    auto &renderNode = userConfig["render"];
    if (!renderNode.is_object()) {
        renderNode = bz::json::Object();
    }
    renderNode["brightness"] = brightnessValue;
}

void RenderSettings::reset() {
    brightnessValue = 1.0f;
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

void RenderSettings::eraseFromConfig(bz::json::Value &userConfig) {
    if (!userConfig.is_object()) {
        return;
    }
    auto renderIt = userConfig.find("render");
    if (renderIt == userConfig.end() || !renderIt->is_object()) {
        return;
    }
    renderIt->erase("brightness");
    if (renderIt->empty()) {
        userConfig.erase("render");
    }
}

} // namespace ui
