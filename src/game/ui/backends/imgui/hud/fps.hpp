#pragma once

struct ImGuiIO;

namespace ui {

class ImGuiHudFps {
public:
    void setVisible(bool show);
    void draw(ImGuiIO &io);

private:
    bool visible = false;
};

} // namespace ui
