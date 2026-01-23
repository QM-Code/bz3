#pragma once

#include <functional>
#include <string>
#include <vector>

#include <RmlUi/Core/Event.h>

#include "engine/components/gui/gui_types.hpp"
#include "engine/components/gui/rmlui_hud_chat.hpp"
#include "engine/components/gui/rmlui_hud_dialog.hpp"
#include "engine/components/gui/rmlui_hud_radar.hpp"
#include "engine/components/gui/rmlui_hud_scoreboard.hpp"

namespace Rml {
class Context;
class ElementDocument;
}

namespace gui {

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

    void setRadarTextureId(unsigned int textureId);
    void setScoreboardEntries(const std::vector<ScoreboardEntry> &entries);

private:
    Rml::Context *context = nullptr;
    Rml::ElementDocument *document = nullptr;
    std::string path;
    EmojiMarkupFn emojiMarkup;

    RmlUiHudDialog dialog;
    RmlUiHudChat chat;
    RmlUiHudRadar radar;
    RmlUiHudScoreboard scoreboard;

    void bindElements();
};

} // namespace gui
