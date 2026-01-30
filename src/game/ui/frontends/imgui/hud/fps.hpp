#pragma once

struct ImGuiIO;

namespace ui {

class ImGuiHudFps {
public:
    void setVisible(bool show);
    void setValue(float value);
    void draw(ImGuiIO &io);

private:
    bool visible = false;
    float fpsValue = 0.0f;
};

} // namespace ui
