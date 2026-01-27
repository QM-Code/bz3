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
            std::string src = "texid:" + std::to_string(textureId);
            if (textureWidth > 0 && textureHeight > 0) {
                src += ":" + std::to_string(textureWidth) + "x" + std::to_string(textureHeight);
            }
            image->SetAttribute("src", src);
        }
    }
}

void RmlUiHudRadar::setTexture(unsigned int textureIdIn, int widthIn, int heightIn) {
    textureId = textureIdIn;
    textureWidth = widthIn;
    textureHeight = heightIn;
    if (image) {
        if (textureId == 0) {
            image->SetAttribute("src", "");
        } else {
            std::string src = "texid:" + std::to_string(textureId);
            if (textureWidth > 0 && textureHeight > 0) {
                src += ":" + std::to_string(textureWidth) + "x" + std::to_string(textureHeight);
            }
            image->SetAttribute("src", src);
        }
    }
}

} // namespace ui
