#pragma once

struct ImVec2;

#include "engine/graphics/texture_handle.hpp"

namespace ui {

class ImGuiHudRadar {
public:
    void setTexture(const graphics::TextureHandle& texture);
    void draw(const ImVec2 &pos, const ImVec2 &size);

private:
    graphics::TextureHandle radarTexture{};
};

} // namespace ui
