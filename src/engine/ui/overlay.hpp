#pragma once

#include "karma/ui/types.hpp"

namespace ui {

class Overlay {
public:
    virtual ~Overlay() = default;

    virtual RenderOutput getRenderOutput() const = 0;
    virtual float getRenderBrightness() const = 0;
};

} // namespace ui
