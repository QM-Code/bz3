#pragma once

#include <string>

#include <RmlUi/Core/Element.h>

namespace Rml {
class ElementDocument;
}

namespace ui {

class RmlUiHudRadar {
public:
    void bind(Rml::ElementDocument *document);
    void setTexture(unsigned int textureId, int width, int height);

private:
    Rml::Element *image = nullptr;
    unsigned int textureId = 0;
    int textureWidth = 0;
    int textureHeight = 0;
};

} // namespace ui
