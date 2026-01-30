#include "ui/frontends/rmlui/hud/scoreboard.hpp"

#include <RmlUi/Core/ElementDocument.h>

namespace ui {

void RmlUiHudScoreboard::bind(Rml::ElementDocument *document, EmojiMarkupFn emojiMarkupIn) {
    emojiMarkup = std::move(emojiMarkupIn);
    container = nullptr;
    if (!document) {
        return;
    }
    container = document->GetElementById("hud-scoreboard");
    if (container) {
        container->SetClass("hidden", !visible);
    }
    rebuild(document);
}

void RmlUiHudScoreboard::setEntries(const std::vector<ScoreboardEntry> &entriesIn) {
    entries = entriesIn;
    if (container) {
        rebuild(container->GetOwnerDocument());
    }
}

void RmlUiHudScoreboard::setVisible(bool visibleIn) {
    visible = visibleIn;
    if (container) {
        container->SetClass("hidden", !visible);
    }
}

bool RmlUiHudScoreboard::isVisible() const {
    return visible;
}

void RmlUiHudScoreboard::rebuild(Rml::ElementDocument *document) {
    if (!container || !document) {
        return;
    }
    while (auto *child = container->GetFirstChild()) {
        container->RemoveChild(child);
    }
    for (const auto &entry : entries) {
        const char *prefix = "  ";
        if (entry.communityAdmin) {
            prefix = "@ ";
        } else if (entry.localAdmin) {
            prefix = "* ";
        } else if (entry.registeredUser) {
            prefix = "+ ";
        }
        std::string line = std::string(prefix) + entry.name + "  (" + std::to_string(entry.score) + ")";
        auto element = document->CreateElement("div");
        element->SetClass("hud-scoreboard-line", true);
        element->SetInnerRML(emojiMarkup ? emojiMarkup(line) : line);
        container->AppendChild(std::move(element));
    }
}

} // namespace ui
