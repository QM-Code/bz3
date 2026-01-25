#pragma once

namespace ui {

class RenderBridge {
public:
    virtual ~RenderBridge() = default;
    virtual unsigned int getRadarTextureId() const = 0;
};

} // namespace ui
