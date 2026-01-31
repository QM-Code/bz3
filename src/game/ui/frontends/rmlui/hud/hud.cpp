#include "ui/frontends/rmlui/hud/hud.hpp"

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/ElementDocument.h>
#include <utility>

#include "karma/common/i18n.hpp"
#include "ui/frontends/rmlui/translate.hpp"

namespace ui {

RmlUiHud::RmlUiHud() = default;

RmlUiHud::~RmlUiHud() {
    unload();
}

void RmlUiHud::load(Rml::Context *contextIn, const std::string &pathIn, EmojiMarkupFn emojiMarkupIn) {
    unload();
    context = contextIn;
    path = pathIn;
    emojiMarkup = std::move(emojiMarkupIn);
    if (!context || path.empty()) {
        return;
    }
    document = context->LoadDocument(path);
    if (!document) {
        return;
    }
    rmlui::ApplyTranslations(document, karma::i18n::Get());
    lastLanguage = karma::i18n::Get().language();
    bindElements();
    document->Show();
}

void RmlUiHud::unload() {
    if (document) {
        document->Close();
        document = nullptr;
        if (context) {
            context->Update();
        }
    }
    context = nullptr;
    path.clear();
    emojiMarkup = nullptr;
}

void RmlUiHud::show() {
    if (document && !document->IsVisible()) {
        document->Show();
    }
}

void RmlUiHud::hide() {
    if (document && document->IsVisible()) {
        document->Hide();
    }
}

bool RmlUiHud::isVisible() const {
    return document && document->IsVisible();
}

void RmlUiHud::update() {
    if (document) {
        const std::string currentLanguage = karma::i18n::Get().language();
        if (currentLanguage != lastLanguage) {
            rmlui::ApplyTranslations(document, karma::i18n::Get());
            lastLanguage = currentLanguage;
            lastFpsInt = -1;
            setFpsValue(lastFps);
        }
    }
    chat.update();
}

void RmlUiHud::setDialogText(const std::string &text) {
    dialog.setText(text);
}

void RmlUiHud::setDialogVisible(bool show) {
    dialog.show(show);
}

void RmlUiHud::setChatLines(const std::vector<std::string> &lines) {
    chat.setLines(lines);
}

void RmlUiHud::addChatLine(const std::string &line) {
    chat.addLine(line);
}

std::string RmlUiHud::getSubmittedChatInput() const {
    return chat.getSubmittedInput();
}

void RmlUiHud::clearSubmittedChatInput() {
    chat.clearSubmittedInput();
}

void RmlUiHud::focusChatInput() {
    chat.focusInput();
}

bool RmlUiHud::isChatFocused() const {
    return chatVisible && chat.isFocused();
}


bool RmlUiHud::consumeSuppressNextChatChar() {
    return chat.consumeSuppressNextChar();
}

void RmlUiHud::handleChatInputEvent(Rml::Event &event) {
    chat.handleInputEvent(event);
}

void RmlUiHud::setRadarTexture(const graphics::TextureHandle& texture) {
    radar.setTexture(texture);
}

void RmlUiHud::setScoreboardEntries(const std::vector<ScoreboardEntry> &entries) {
    scoreboard.setEntries(entries);
}

void RmlUiHud::setScoreboardVisible(bool visible) {
    scoreboardVisible = visible;
    scoreboard.setVisible(visible);
}

void RmlUiHud::setChatVisible(bool visible) {
    chatVisible = visible;
    chat.setVisible(visible);
}

void RmlUiHud::setRadarVisible(bool visible) {
    radarVisible = visible;
    radar.setVisible(visible);
}

void RmlUiHud::setCrosshairVisible(bool visible) {
    crosshairVisible = visible;
    crosshair.setVisible(visible);
}

void RmlUiHud::setFpsVisible(bool visible) {
    if (visible == fpsVisible) {
        return;
    }
    fpsVisible = visible;
    if (fpsElement) {
        fpsElement->SetClass("hidden", !visible);
    }
}

void RmlUiHud::setFpsValue(float fps) {
    lastFps = fps;
    if (fpsElement) {
        const int fpsInt = static_cast<int>(fps + 0.5f);
        if (fpsInt == lastFpsInt) {
            return;
        }
        lastFpsInt = fpsInt;
        const std::string value = std::to_string(fpsInt);
        const std::string fpsText = karma::i18n::Get().format("ui.hud.fps_label", {{"value", value}});
        fpsElement->SetInnerRML(fpsText);
    }
}

void RmlUiHud::bindElements() {
    if (!document) {
        return;
    }
    dialog.bind(document, emojiMarkup);
    chat.bind(document, emojiMarkup);
    crosshair.bind(document);
    radar.bind(document);
    scoreboard.bind(document, emojiMarkup);
    chat.setVisible(chatVisible);
    scoreboard.setVisible(scoreboardVisible);
    radar.setVisible(radarVisible);
    crosshair.setVisible(crosshairVisible);
    fpsElement = document->GetElementById("hud-fps");
    fpsVisible = fpsElement && !fpsElement->IsClassSet("hidden");
    setFpsValue(lastFps);
}

} // namespace ui
