#pragma once

#include <functional>
#include <string>
#include <vector>

#include <RmlUi/Core/Event.h>

#include "ui/types.hpp"
#include "ui/backends/rmlui/hud/chat.hpp"
#include "ui/backends/rmlui/hud/dialog.hpp"
#include "ui/backends/rmlui/hud/radar.hpp"
#include "ui/backends/rmlui/hud/scoreboard.hpp"

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
    void showDialog(bool show);

    void addChatLine(const std::string &line);
    std::string getSubmittedChatInput() const;
    void clearSubmittedChatInput();
    void focusChatInput();
    bool isChatFocused() const;
    bool consumeSuppressNextChatChar();
    void handleChatInputEvent(Rml::Event &event);

    void setRadarTexture(unsigned int textureId, int width, int height);
    void setScoreboardEntries(const std::vector<ScoreboardEntry> &entries);
    void setFpsVisible(bool visible);
    void setFpsValue(float fps);

private:
    Rml::Context *context = nullptr;
    Rml::ElementDocument *document = nullptr;
    std::string path;
    EmojiMarkupFn emojiMarkup;

    RmlUiHudDialog dialog;
    RmlUiHudChat chat;
    RmlUiHudRadar radar;
    RmlUiHudScoreboard scoreboard;
    Rml::Element *fpsElement = nullptr;
    float lastFps = 0.0f;
    int lastFpsInt = -1;
    bool fpsVisible = false;

    void bindElements();
};

} // namespace ui
