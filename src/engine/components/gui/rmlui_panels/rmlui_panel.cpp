#include "engine/components/gui/rmlui_panels/rmlui_panel.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>

#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <spdlog/spdlog.h>

#include "common/data_path_resolver.hpp"

namespace gui {

RmlUiPanel::RmlUiPanel(std::string key, std::string rmlPath)
    : panelKey(std::move(key)), panelRmlPath(std::move(rmlPath)) {}

const std::string &RmlUiPanel::key() const {
    return panelKey;
}

void RmlUiPanel::load(Rml::ElementDocument *document) {
    if (!document) {
        return;
    }
    const std::string panelId = "panel-" + panelKey;
    Rml::Element *panel = document->GetElementById(panelId);
    if (!panel) {
        return;
    }

    const auto resolvedPath = bz::data::Resolve(panelRmlPath);
    if (resolvedPath.empty() || !std::filesystem::exists(resolvedPath)) {
        return;
    }
    std::ifstream file(resolvedPath);
    if (!file) {
        spdlog::warn("RmlUi: failed to open panel file '{}'.", resolvedPath.string());
        return;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    panel->SetInnerRML(buffer.str());
    onLoaded(document);
}

void RmlUiPanel::onLoaded(Rml::ElementDocument *) {}

void RmlUiPanel::update() {
    onUpdate();
}

void RmlUiPanel::onUpdate() {}

} // namespace gui
