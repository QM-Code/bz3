#pragma once

#include <functional>
#include <string>
#include <vector>

#include <RmlUi/Core/Element.h>

#include "ui/types.hpp"

namespace Rml {
class ElementDocument;
}

namespace ui {

class RmlUiHudScoreboard {
public:
    using EmojiMarkupFn = std::function<const std::string &(const std::string &)>;

    void bind(Rml::ElementDocument *document, EmojiMarkupFn emojiMarkupIn);
    void setEntries(const std::vector<ScoreboardEntry> &entries);

private:
    Rml::Element *container = nullptr;
    std::vector<ScoreboardEntry> entries;
    EmojiMarkupFn emojiMarkup;

    void rebuild(Rml::ElementDocument *document);
};

} // namespace ui
