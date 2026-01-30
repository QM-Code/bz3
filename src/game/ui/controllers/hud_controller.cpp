#include "ui/controllers/hud_controller.hpp"

namespace ui {

HudController::HudController(HudModel &modelIn)
    : model(modelIn) {}

void HudController::tick() {
    const auto now = std::chrono::steady_clock::now();
    if (!hasTick) {
        lastTick = now;
        hasTick = true;
        return;
    }

    const std::chrono::duration<float> delta = now - lastTick;
    lastTick = now;
    const float dt = delta.count();
    if (dt <= 0.0f) {
        return;
    }

    const float fpsInstant = 1.0f / dt;
    if (fpsSmoothed <= 0.0f) {
        fpsSmoothed = fpsInstant;
    } else {
        const float alpha = 0.15f;
        fpsSmoothed = fpsSmoothed + alpha * (fpsInstant - fpsSmoothed);
    }
    model.fpsValue = fpsSmoothed;
}

void HudController::resetFps() {
    hasTick = false;
    fpsSmoothed = 0.0f;
    model.fpsValue = 0.0f;
}

void HudController::setDialogText(const std::string &text) {
    model.dialog.text = text;
}

void HudController::setDialogVisible(bool visible) {
    model.dialog.visible = visible;
}

void HudController::addChatLine(const std::string &playerName, const std::string &line) {
    model.chatLines.push_back(formatChatLine(playerName, line));
}

std::string HudController::formatChatLine(const std::string &playerName, const std::string &line) {
    if (playerName.empty()) {
        return line;
    }
    if (!playerName.empty() && playerName.front() == '[') {
        return playerName + " " + line;
    }
    return "[" + playerName + "] " + line;
}

} // namespace ui
