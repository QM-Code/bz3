#include "ui/render_scale.hpp"

#include "common/config_store.hpp"
#include "spdlog/spdlog.h"

#include <algorithm>

namespace ui {
namespace {

constexpr float kMinRenderScale = 0.5f;
constexpr float kMaxRenderScale = 1.0f;

}

float GetUiRenderScale() {
    static uint64_t lastRevision = 0;
    static float cachedScale = 1.0f;
    const uint64_t revision = bz::config::ConfigStore::Revision();
    if (revision != lastRevision) {
        lastRevision = revision;
        const auto* value = bz::config::ConfigStore::Get("ui.RenderScale");
        if (!value) {
            spdlog::error("Config 'ui.RenderScale' is missing");
            cachedScale = 1.0f;
            return cachedScale;
        }

        float scale = 1.0f;
        if (value->is_number_float()) {
            scale = static_cast<float>(value->get<double>());
        } else if (value->is_number_integer()) {
            scale = static_cast<float>(value->get<long long>());
        } else if (value->is_string()) {
            try {
                scale = std::stof(value->get<std::string>());
            } catch (...) {
                spdlog::error("Config 'ui.RenderScale' string value is not a valid float");
            }
        } else {
            spdlog::error("Config 'ui.RenderScale' cannot be interpreted as float");
        }

        const float clamped = std::clamp(scale, kMinRenderScale, kMaxRenderScale);
        if (clamped != scale) {
            spdlog::warn("Config 'ui.RenderScale' clamped from {} to {}", scale, clamped);
        }
        cachedScale = clamped;
    }
    return cachedScale;
}

} // namespace ui
