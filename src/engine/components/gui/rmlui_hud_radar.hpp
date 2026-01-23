#pragma once

#include <string>

#include <RmlUi/Core/Element.h>

namespace Rml {
class ElementDocument;
}

namespace gui {

class RmlUiHudRadar {
public:
    void bind(Rml::ElementDocument *document);
    void setTextureId(unsigned int textureId);

private:
    Rml::Element *image = nullptr;
    unsigned int textureId = 0;
};

} // namespace gui
