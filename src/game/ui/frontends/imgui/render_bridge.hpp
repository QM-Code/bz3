#pragma once

#include "engine/graphics/ui_bridge.hpp"

namespace ui {

class ImGuiRenderBridge {
public:
    virtual ~ImGuiRenderBridge() = default;
    virtual graphics_backend::ImGuiBridge* getImGuiBridge() const = 0;
};

} // namespace ui
