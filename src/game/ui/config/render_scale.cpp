#include "ui/config/render_scale.hpp"

#include "common/config_store.hpp"
#include "spdlog/spdlog.h"
#include "ui/config/ui_config.hpp"

#include <algorithm>

namespace ui {

float GetUiRenderScale() {
    static uint64_t lastRevision = 0;
    static float cachedScale = UiConfig::kDefaultRenderScale;
    const uint64_t revision = bz::config::ConfigStore::Revision();
    if (revision != lastRevision) {
        lastRevision = revision;
        const auto value = UiConfig::TryGetRenderScale();
        if (!value) {
            spdlog::error("Config 'ui.RenderScale' is missing");
            cachedScale = UiConfig::kDefaultRenderScale;
            return cachedScale;
        }
        const float clamped = std::clamp(*value, UiConfig::kMinRenderScale, UiConfig::kMaxRenderScale);
        if (clamped != *value) {
            spdlog::warn("Config 'ui.RenderScale' clamped from {} to {}", *value, clamped);
        }
        cachedScale = clamped;
    }
    return cachedScale;
}

} // namespace ui
