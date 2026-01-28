#pragma once

#include "engine/graphics/ui_render_target_bridge.hpp"

namespace ui {

class ImGuiRenderBridge {
public:
    virtual ~ImGuiRenderBridge() = default;
    virtual graphics_backend::UiRenderTargetBridge* getUiRenderTargetBridge() const = 0;
};

} // namespace ui
