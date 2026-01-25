#pragma once

#include <functional>
#include <string>
#include <vector>

#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/Event.h>
#include <RmlUi/Core/EventListener.h>

namespace Rml {
class ElementDocument;
class EventListener;
}

namespace ui {

class RmlUiHudChat {
public:
    using EmojiMarkupFn = std::function<const std::string &(const std::string &)>;

    void bind(Rml::ElementDocument *document, EmojiMarkupFn emojiMarkupIn);
    void update();

    void addLine(const std::string &line);
    std::string getSubmittedInput() const;
    void clearSubmittedInput();

    void focusInput();
    bool isFocused() const;
    bool consumeSuppressNextChar();
    void handleInputEvent(Rml::Event &event);

private:
    Rml::Element *panel = nullptr;
    Rml::Element *log = nullptr;
    Rml::Element *logContent = nullptr;
    Rml::Element *input = nullptr;
    std::unique_ptr<Rml::EventListener> inputListener;

    std::vector<std::string> lines;
    std::string submittedInput;
    bool focused = false;
    bool autoScroll = true;
    bool pendingScroll = false;
    bool suppressNextChar = false;

    EmojiMarkupFn emojiMarkup;

    void rebuildLines(Rml::ElementDocument *document);
};

} // namespace ui
