#include "ui/backends/rmlui/hud/hud.hpp"

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/ElementDocument.h>
#include "common/i18n.hpp"
#include "ui/backends/rmlui/translate.hpp"
#include <utility>

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
    rmlui::ApplyTranslations(document, bz::i18n::Get());
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
    chat.update();
}

void RmlUiHud::setDialogText(const std::string &text) {
    dialog.setText(text);
}

void RmlUiHud::showDialog(bool show) {
    dialog.show(show);
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
    return chat.isFocused();
}


bool RmlUiHud::consumeSuppressNextChatChar() {
    return chat.consumeSuppressNextChar();
}

void RmlUiHud::handleChatInputEvent(Rml::Event &event) {
    chat.handleInputEvent(event);
}

void RmlUiHud::setRadarTextureId(unsigned int textureId) {
    radar.setTextureId(textureId);
}

void RmlUiHud::setScoreboardEntries(const std::vector<ScoreboardEntry> &entries) {
    scoreboard.setEntries(entries);
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
        const std::string fpsText = bz::i18n::Get().format("ui.hud.fps_label", {{"value", value}});
        fpsElement->SetInnerRML(fpsText);
    }
}

void RmlUiHud::bindElements() {
    if (!document) {
        return;
    }
    dialog.bind(document, emojiMarkup);
    chat.bind(document, emojiMarkup);
    radar.bind(document);
    scoreboard.bind(document, emojiMarkup);
    fpsElement = document->GetElementById("hud-fps");
    fpsVisible = fpsElement && !fpsElement->IsClassSet("hidden");
    setFpsValue(lastFps);
}

} // namespace ui
