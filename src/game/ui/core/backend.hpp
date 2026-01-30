#pragma once

#include <memory>
#include <string>
#include <vector>

#include "platform/events.hpp"
#include "ui/models/hud_model.hpp"
#include "ui/core/types.hpp"
#include "engine/ui/bridges/render_bridge.hpp"
#include "ui/console/console_interface.hpp"

namespace platform {
class Window;
}

namespace ui_backend {

class Backend {
public:
    virtual ~Backend() = default;

    virtual ui::ConsoleInterface &console() = 0;
    virtual const ui::ConsoleInterface &console() const = 0;
    virtual void handleEvents(const std::vector<platform::Event> &events) = 0;
    virtual void update() = 0;
    virtual void reloadFonts() = 0;

    virtual void setHudModel(const ui::HudModel &model) = 0;
    virtual void addConsoleLine(const std::string &playerName, const std::string &line) = 0;
    virtual std::string getChatInputBuffer() const = 0;
    virtual void clearChatInputBuffer() = 0;
    virtual void focusChatInput() = 0;
    virtual bool getChatInputFocus() const = 0;
    virtual bool consumeKeybindingsReloadRequest() = 0;
    virtual void setRenderBridge(const ui::RenderBridge *bridge) = 0;
    virtual ui::RenderOutput getRenderOutput() const { return {}; }
    virtual float getRenderBrightness() const { return 1.0f; }
    virtual bool isRenderBrightnessDragActive() const { return false; }
};

std::unique_ptr<Backend> CreateUiBackend(platform::Window &window);

} // namespace ui_backend
