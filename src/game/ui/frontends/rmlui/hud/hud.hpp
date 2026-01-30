#pragma once

#include <functional>
#include <string>
#include <vector>

#include <RmlUi/Core/Event.h>

#include "ui/core/types.hpp"
#include "ui/frontends/rmlui/hud/chat.hpp"
#include "ui/frontends/rmlui/hud/crosshair.hpp"
#include "ui/frontends/rmlui/hud/dialog.hpp"
#include "ui/frontends/rmlui/hud/radar.hpp"
#include "ui/frontends/rmlui/hud/scoreboard.hpp"

namespace Rml {
class Context;
class ElementDocument;
}

namespace ui {

class RmlUiHud {
public:
    using EmojiMarkupFn = std::function<const std::string &(const std::string &)>;

    RmlUiHud();
    ~RmlUiHud();

    void load(Rml::Context *contextIn, const std::string &pathIn, EmojiMarkupFn emojiMarkupIn);
    void unload();
    void show();
    void hide();
    bool isVisible() const;

    void update();

    void setDialogText(const std::string &text);
    void setDialogVisible(bool show);
    void setChatLines(const std::vector<std::string> &lines);

    void addChatLine(const std::string &line);
    std::string getSubmittedChatInput() const;
    void clearSubmittedChatInput();
    void focusChatInput();
    bool isChatFocused() const;
    bool consumeSuppressNextChatChar();
    void handleChatInputEvent(Rml::Event &event);

    void setRadarTexture(const graphics::TextureHandle& texture);
    void setScoreboardEntries(const std::vector<ScoreboardEntry> &entries);
    void setScoreboardVisible(bool visible);
    void setChatVisible(bool visible);
    void setRadarVisible(bool visible);
    void setCrosshairVisible(bool visible);
    void setFpsVisible(bool visible);
    void setFpsValue(float fps);

private:
    Rml::Context *context = nullptr;
    Rml::ElementDocument *document = nullptr;
    std::string path;
    EmojiMarkupFn emojiMarkup;

    RmlUiHudDialog dialog;
    RmlUiHudChat chat;
    RmlUiHudCrosshair crosshair;
    RmlUiHudRadar radar;
    RmlUiHudScoreboard scoreboard;
    Rml::Element *fpsElement = nullptr;
    float lastFps = 0.0f;
    int lastFpsInt = -1;
    bool fpsVisible = false;
    bool scoreboardVisible = true;
    bool chatVisible = true;
    bool radarVisible = true;
    bool crosshairVisible = true;
    std::string lastLanguage;

    void bindElements();
};

} // namespace ui
