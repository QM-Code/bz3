#include "ui/backends/rmlui/hud/radar.hpp"

#include <RmlUi/Core/ElementDocument.h>

namespace ui {

void RmlUiHudRadar::bind(Rml::ElementDocument *document) {
    image = nullptr;
    if (!document) {
        return;
    }
    image = document->GetElementById("hud-radar-image");
    if (image) {
        if (textureId == 0) {
            image->SetAttribute("src", "");
        } else {
            image->SetAttribute("src", "texid:" + std::to_string(textureId));
        }
    }
}

void RmlUiHudRadar::setTextureId(unsigned int textureIdIn) {
    textureId = textureIdIn;
    if (image) {
        if (textureId == 0) {
            image->SetAttribute("src", "");
        } else {
            image->SetAttribute("src", "texid:" + std::to_string(textureId));
        }
    }
}

} // namespace ui
