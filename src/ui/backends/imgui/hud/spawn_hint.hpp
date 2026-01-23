#pragma once

#include <string>

struct ImFont;
struct ImGuiIO;

namespace ui {

class ImGuiHudSpawnHint {
public:
    void setHint(const std::string &hint);
    void setVisible(bool show);
    void draw(ImGuiIO &io, ImFont *bigFont);

private:
    std::string hintText = "Press U to spawn";
    bool visible = false;
};

} // namespace ui
