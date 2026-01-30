#pragma once

#include <chrono>

#include "ui/models/hud_model.hpp"

namespace ui {

class HudController {
public:
    explicit HudController(HudModel &model);

    void tick();
    void resetFps();
    void setDialogText(const std::string &text);
    void setDialogVisible(bool visible);
    void addChatLine(const std::string &playerName, const std::string &line);

private:
    HudModel &model;
    std::chrono::steady_clock::time_point lastTick{};
    bool hasTick = false;
    float fpsSmoothed = 0.0f;

    static std::string formatChatLine(const std::string &playerName, const std::string &line);
};

} // namespace ui
