#pragma once

#include <string>

namespace Rml {
class ElementDocument;
}

namespace ui {

class RmlUiPanel {
public:
    RmlUiPanel(std::string key, std::string rmlPath);
    virtual ~RmlUiPanel() = default;

    const std::string &key() const;
    void load(Rml::ElementDocument *document);
    void update();

protected:
    virtual void onLoaded(Rml::ElementDocument *document);
    virtual void onUpdate();

private:
    std::string panelKey;
    std::string panelRmlPath;
};

} // namespace ui
