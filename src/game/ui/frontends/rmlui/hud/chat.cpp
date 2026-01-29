#include "ui/frontends/rmlui/hud/chat.hpp"

#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/EventListener.h>
#include <RmlUi/Core/Elements/ElementFormControlInput.h>
#include <RmlUi/Core/Input.h>
#include <utility>

namespace ui {
namespace {

class ChatInputListener final : public Rml::EventListener {
public:
    explicit ChatInputListener(RmlUiHudChat *chatIn) : chat(chatIn) {}

    void ProcessEvent(Rml::Event &event) override {
        if (chat) {
            chat->handleInputEvent(event);
        }
    }

private:
    RmlUiHudChat *chat = nullptr;
};

} // namespace

void RmlUiHudChat::bind(Rml::ElementDocument *document, EmojiMarkupFn emojiMarkupIn) {
    emojiMarkup = std::move(emojiMarkupIn);
    panel = nullptr;
    log = nullptr;
    logContent = nullptr;
    input = nullptr;
    inputListener.reset();
    if (!document) {
        return;
    }
    panel = document->GetElementById("hud-chat-panel");
    log = document->GetElementById("hud-chat-log");
    logContent = document->GetElementById("hud-chat-log-content");
    input = document->GetElementById("hud-chat-input");

    if (panel) {
        panel->SetClass("hidden", !visible);
    }

    if (input) {
        inputListener = std::make_unique<ChatInputListener>(this);
        input->AddEventListener("keydown", inputListener.get());
        input->AddEventListener("focus", inputListener.get());
        input->AddEventListener("blur", inputListener.get());
    }

    rebuildLines(document);
}

void RmlUiHudChat::update() {
    if (!log) {
        return;
    }
    const float scrollHeight = log->GetScrollHeight();
    const float viewHeight = log->GetOffsetHeight();
    const float scrollMax = std::max(0.0f, scrollHeight - viewHeight);
    const float scrollTop = log->GetScrollTop();
    const float atBottomEpsilon = 2.0f;
    if (scrollMax > 0.0f) {
        autoScroll = (scrollTop >= scrollMax - atBottomEpsilon);
    } else {
        autoScroll = true;
    }
    if (pendingScroll || autoScroll) {
        log->SetScrollTop(scrollMax);
        pendingScroll = false;
    }
}

void RmlUiHudChat::addLine(const std::string &line) {
    std::size_t start = 0;
    while (start <= line.size()) {
        std::size_t end = line.find('\n', start);
        if (end == std::string::npos) {
            end = line.size();
        }
        std::string segment = line.substr(start, end - start);
        if (!segment.empty() && segment.back() == '\r') {
            segment.pop_back();
        }
        lines.push_back(segment);
        if (logContent && logContent->GetOwnerDocument()) {
            auto lineElement = logContent->GetOwnerDocument()->CreateElement("div");
            lineElement->SetClass("hud-chat-line", true);
            lineElement->SetInnerRML(emojiMarkup ? emojiMarkup(segment) : segment);
            logContent->AppendChild(std::move(lineElement));
            pendingScroll = true;
        }
        if (end >= line.size()) {
            break;
        }
        start = end + 1;
    }
}

std::string RmlUiHudChat::getSubmittedInput() const {
    return submittedInput;
}

void RmlUiHudChat::clearSubmittedInput() {
    submittedInput.clear();
}

void RmlUiHudChat::focusInput() {
    focused = true;
    suppressNextChar = true;
    if (input) {
        input->Focus();
    }
}

bool RmlUiHudChat::isFocused() const {
    return focused;
}

void RmlUiHudChat::setVisible(bool visibleIn) {
    visible = visibleIn;
    if (panel) {
        panel->SetClass("hidden", !visible);
    }
    if (!visible) {
        focused = false;
    }
}

bool RmlUiHudChat::isVisible() const {
    return visible;
}


bool RmlUiHudChat::consumeSuppressNextChar() {
    if (!suppressNextChar) {
        return false;
    }
    suppressNextChar = false;
    return true;
}

void RmlUiHudChat::handleInputEvent(Rml::Event &event) {
    if (event.GetType() == "focus") {
        focused = true;
        return;
    }
    if (event.GetType() == "blur") {
        focused = false;
        return;
    }
    if (!input) {
        return;
    }
    auto *control = rmlui_dynamic_cast<Rml::ElementFormControlInput *>(input);
    if (!control) {
        return;
    }
    const int keyIdentifier = event.GetParameter<int>("key_identifier", Rml::Input::KI_UNKNOWN);
    if (keyIdentifier == Rml::Input::KI_ESCAPE) {
        control->SetValue("");
        submittedInput.clear();
        focused = false;
        return;
    }
    if (keyIdentifier == Rml::Input::KI_RETURN || keyIdentifier == Rml::Input::KI_NUMPADENTER) {
        const Rml::String value = control->GetValue();
        submittedInput = value;
        control->SetValue("");
        focused = true;
        control->Focus();
    }
}

void RmlUiHudChat::rebuildLines(Rml::ElementDocument *document) {
    if (!logContent || !document) {
        return;
    }
    while (auto *child = logContent->GetFirstChild()) {
        logContent->RemoveChild(child);
    }
    for (const auto &line : lines) {
        auto lineElement = document->CreateElement("div");
        lineElement->SetClass("hud-chat-line", true);
        lineElement->SetInnerRML(emojiMarkup ? emojiMarkup(line) : line);
        logContent->AppendChild(std::move(lineElement));
    }
    pendingScroll = true;
}

} // namespace ui
