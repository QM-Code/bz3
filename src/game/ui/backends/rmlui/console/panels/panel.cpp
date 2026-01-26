#include "ui/backends/rmlui/console/panels/panel.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>

#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <spdlog/spdlog.h>

#include "common/data_path_resolver.hpp"
#include "common/i18n.hpp"
#include "ui/backends/rmlui/translate.hpp"

namespace ui {

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
    rmlui::ApplyTranslations(panel, bz::i18n::Get());
    onLoaded(document);
}

void RmlUiPanel::onLoaded(Rml::ElementDocument *) {}

void RmlUiPanel::update() {
    onUpdate();
}

void RmlUiPanel::onUpdate() {}

} // namespace ui
