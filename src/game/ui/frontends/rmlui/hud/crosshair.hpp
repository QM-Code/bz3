#pragma once

namespace Rml {
class ElementDocument;
class Element;
}

namespace ui {

class RmlUiHudCrosshair {
public:
    void bind(Rml::ElementDocument *document);
    void setVisible(bool visible);
    bool isVisible() const;

private:
    Rml::Element *element = nullptr;
    bool visible = false;
};

} // namespace ui
