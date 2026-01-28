#include "ui/frontends/rmlui/console/modal_dialog.hpp"

#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/EventListener.h>
#include <RmlUi/Core/Input.h>

namespace ui {
namespace {
enum class DialogAction {
    Accept,
    Cancel
};
}

class RmlUiModalDialog::ButtonListener final : public Rml::EventListener {
public:
    ButtonListener(RmlUiModalDialog *dialogIn, DialogAction actionIn)
        : dialog(dialogIn), action(actionIn) {}

    void ProcessEvent(Rml::Event &) override {
        if (!dialog) {
            return;
        }
        if (action == DialogAction::Accept) {
            dialog->handleAccept();
        } else {
            dialog->handleCancel();
        }
    }

private:
    RmlUiModalDialog *dialog = nullptr;
    DialogAction action;
};

class RmlUiModalDialog::KeyListener final : public Rml::EventListener {
public:
    explicit KeyListener(RmlUiModalDialog *dialogIn)
        : dialog(dialogIn) {}

    void ProcessEvent(Rml::Event &event) override {
        if (!dialog || !dialog->isVisible()) {
            return;
        }
        const int keyIdentifier = event.GetParameter<int>("key_identifier", Rml::Input::KI_UNKNOWN);
        if (keyIdentifier == Rml::Input::KI_ESCAPE) {
            dialog->handleCancel();
        }
    }

private:
    RmlUiModalDialog *dialog = nullptr;
};

void RmlUiModalDialog::bind(Rml::ElementDocument *documentIn,
                            const std::string &overlayId,
                            const std::string &messageId,
                            const std::string &acceptButtonId,
                            const std::string &cancelButtonId) {
    document = documentIn;
    overlay = document ? document->GetElementById(overlayId) : nullptr;
    message = document ? document->GetElementById(messageId) : nullptr;
    acceptButton = document ? document->GetElementById(acceptButtonId) : nullptr;
    cancelButton = (!cancelButtonId.empty() && document) ? document->GetElementById(cancelButtonId) : nullptr;
}

void RmlUiModalDialog::installListeners(std::vector<std::unique_ptr<Rml::EventListener>> &listeners) {
    if (acceptButton) {
        auto listener = std::make_unique<ButtonListener>(this, DialogAction::Accept);
        acceptButton->AddEventListener("click", listener.get());
        listeners.emplace_back(std::move(listener));
    }
    if (cancelButton) {
        auto listener = std::make_unique<ButtonListener>(this, DialogAction::Cancel);
        cancelButton->AddEventListener("click", listener.get());
        listeners.emplace_back(std::move(listener));
    }
    if (document) {
        auto listener = std::make_unique<KeyListener>(this);
        document->AddEventListener("keydown", listener.get());
        listeners.emplace_back(std::move(listener));
    }
}

void RmlUiModalDialog::setOnAccept(Callback callback) {
    onAccept = std::move(callback);
}

void RmlUiModalDialog::setOnCancel(Callback callback) {
    onCancel = std::move(callback);
}

void RmlUiModalDialog::show(const std::string &messageRml) {
    if (message) {
        message->SetInnerRML(messageRml);
    }
    if (overlay) {
        overlay->SetClass("hidden", false);
    }
    if (acceptButton) {
        acceptButton->Focus();
    } else if (overlay) {
        overlay->Focus();
    }
}

void RmlUiModalDialog::hide() {
    if (overlay) {
        overlay->SetClass("hidden", true);
    }
}

bool RmlUiModalDialog::isVisible() const {
    return overlay && !overlay->IsClassSet("hidden");
}

void RmlUiModalDialog::handleAccept() {
    hide();
    if (onAccept) {
        onAccept();
    }
}

void RmlUiModalDialog::handleCancel() {
    hide();
    if (onCancel) {
        onCancel();
    }
}

} // namespace ui
