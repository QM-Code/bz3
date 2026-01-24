#pragma once

struct ImVec2;

namespace ui {

class ImGuiHudRadar {
public:
    void setTextureId(unsigned int textureId);
    void draw(const ImVec2 &pos, const ImVec2 &size);

private:
    unsigned int radarTextureId = 0;
};

} // namespace ui
