#pragma once

#include "ui/config/hud_settings.hpp"
#include "ui/config/render_settings.hpp"

#include <cstdint>
#include <string>

namespace ui {

struct SettingsModel {
    RenderSettings render;
    HudSettings hud;
    std::string language;
    std::string statusText;
    bool statusIsError = false;
    uint64_t lastConfigRevision = 0;
    bool loaded = false;
};

} // namespace ui
