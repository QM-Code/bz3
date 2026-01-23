#include "engine/components/gui/rmlui_hud_dialog.hpp"

#include <RmlUi/Core/ElementDocument.h>
#include <utility>

namespace gui {

void RmlUiHudDialog::bind(Rml::ElementDocument *document, EmojiMarkupFn emojiMarkupIn) {
    emojiMarkup = std::move(emojiMarkupIn);
    overlay = nullptr;
    textElement = nullptr;
    if (!document) {
        return;
    }
    overlay = document->GetElementById("hud-dialog-overlay");
    textElement = document->GetElementById("hud-dialog-text");
    if (textElement && emojiMarkup) {
        textElement->SetInnerRML(emojiMarkup(currentText));
    }
    if (overlay) {
        overlay->SetClass("hidden", !visible);
    }
}

void RmlUiHudDialog::setText(const std::string &text) {
    currentText = text;
    if (textElement) {
        if (emojiMarkup) {
            textElement->SetInnerRML(emojiMarkup(currentText));
        } else {
            textElement->SetInnerRML(currentText);
        }
    }
}

void RmlUiHudDialog::show(bool visibleIn) {
    visible = visibleIn;
    if (overlay) {
        overlay->SetClass("hidden", !visible);
    }
}

} // namespace gui
