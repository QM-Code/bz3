#include "ui/frontends/rmlui/hud/crosshair.hpp"

#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>

namespace ui {

void RmlUiHudCrosshair::bind(Rml::ElementDocument *document) {
    if (!document) {
        element = nullptr;
        visible = false;
        return;
    }
    element = document->GetElementById("hud-crosshair");
    visible = element && !element->IsClassSet("hidden");
}

void RmlUiHudCrosshair::setVisible(bool nextVisible) {
    if (visible == nextVisible) {
        return;
    }
    visible = nextVisible;
    if (element) {
        element->SetClass("hidden", !visible);
    }
}

bool RmlUiHudCrosshair::isVisible() const {
    return visible;
}

} // namespace ui
