#pragma once

struct ImGuiIO;
struct ImVec4;

namespace ui {

class ImGuiHudFps {
public:
    void setVisible(bool show);
    void setValue(float value);
    void draw(ImGuiIO &io, const ImVec4 &backgroundColor);

private:
    bool visible = false;
    float fpsValue = 0.0f;
};

} // namespace ui
