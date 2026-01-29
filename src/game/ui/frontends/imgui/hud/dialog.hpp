#pragma once

#include <string>

struct ImFont;
struct ImGuiIO;

namespace ui {

class ImGuiHudDialog {
public:
    void setText(const std::string &text);
    void setVisible(bool show);
    void draw(ImGuiIO &io, ImFont *bigFont);

private:
    std::string dialogText = "Press U to spawn";
    bool visible = false;
};

} // namespace ui
