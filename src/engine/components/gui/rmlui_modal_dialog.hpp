#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace Rml {
class Element;
class ElementDocument;
class EventListener;
}

namespace gui {

class RmlUiModalDialog {
public:
    using Callback = std::function<void()>;

    void bind(Rml::ElementDocument *document,
              const std::string &overlayId,
              const std::string &messageId,
              const std::string &acceptButtonId,
              const std::string &cancelButtonId = {});
    void installListeners(std::vector<std::unique_ptr<Rml::EventListener>> &listeners);
    void setOnAccept(Callback callback);
    void setOnCancel(Callback callback);
    void show(const std::string &messageRml);
    void hide();
    bool isVisible() const;

private:
    class ButtonListener;
    class KeyListener;

    void handleAccept();
    void handleCancel();

    Rml::ElementDocument *document = nullptr;
    Rml::Element *overlay = nullptr;
    Rml::Element *message = nullptr;
    Rml::Element *acceptButton = nullptr;
    Rml::Element *cancelButton = nullptr;
    Callback onAccept;
    Callback onCancel;
};

} // namespace gui
