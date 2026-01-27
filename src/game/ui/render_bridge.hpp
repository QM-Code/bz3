#pragma once

namespace ui {

class RenderBridge {
public:
    virtual ~RenderBridge() = default;
    virtual unsigned int getRadarTextureId() const = 0;
    virtual void getRadarTextureSize(int& width, int& height) const = 0;
};

} // namespace ui
