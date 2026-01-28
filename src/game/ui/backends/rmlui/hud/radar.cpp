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
        if (!texture.valid()) {
            image->SetAttribute("src", "");
        } else {
            std::string src = "texid:" + std::to_string(texture.id);
            if (texture.width > 0 && texture.height > 0) {
                src += ":" + std::to_string(texture.width) + "x" + std::to_string(texture.height);
            }
            image->SetAttribute("src", src);
        }
    }
}

void RmlUiHudRadar::setTexture(const graphics::TextureHandle& textureIn) {
    texture = textureIn;
    if (image) {
        if (!texture.valid()) {
            image->SetAttribute("src", "");
        } else {
            std::string src = "texid:" + std::to_string(texture.id);
            if (texture.width > 0 && texture.height > 0) {
                src += ":" + std::to_string(texture.width) + "x" + std::to_string(texture.height);
            }
            image->SetAttribute("src", src);
        }
    }
}

} // namespace ui
