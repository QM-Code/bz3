#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "common/config_helpers.hpp"
#include "platform/events.hpp"

namespace platform {

struct WindowConfig {
    int width = static_cast<int>(karma::config::ReadRequiredUInt16Config("platform.WindowWidth"));
    int height = static_cast<int>(karma::config::ReadRequiredUInt16Config("platform.WindowHeight"));
    // Engine default comes from config; game config should override this.
    std::string title = karma::config::ReadRequiredStringConfig("platform.WindowTitle");
    std::string preferredVideoDriver;
};

class Window {
public:
    virtual ~Window() = default;

    virtual void pollEvents() = 0;
    virtual const std::vector<Event>& events() const = 0;
    virtual void clearEvents() = 0;

    virtual bool shouldClose() const = 0;
    virtual void requestClose() = 0;

    virtual void setVsync(bool enabled) = 0;
    virtual void setFullscreen(bool enabled) = 0;
    virtual bool isFullscreen() const = 0;

    virtual void getFramebufferSize(int &width, int &height) const = 0;
    virtual float getContentScale() const = 0;

    virtual bool isKeyDown(Key key) const = 0;
    virtual bool isMouseDown(MouseButton button) const = 0;

    virtual void setCursorVisible(bool visible) = 0;
    virtual void setClipboardText(std::string_view text) = 0;
    virtual std::string getClipboardText() const = 0;

    virtual void setBrightness(float brightness) = 0;

    virtual void* nativeHandle() const = 0;
    virtual std::string getVideoDriver() const = 0;
};

std::unique_ptr<Window> CreateWindow(const WindowConfig &config);
std::unique_ptr<Window> CreateSdl3Window(const WindowConfig &config);
std::unique_ptr<Window> CreateSdl2Window(const WindowConfig &config);

} // namespace platform
