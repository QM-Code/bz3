#include "engine/components/gui/main_menu.hpp"

namespace gui {

void MainMenuView::drawDocumentationPanel(const MessageColors &colors) const {
    drawPlaceholderPanel(
        "Documentation",
        "Browse manuals, onboarding tips, and gameplay references.",
        colors);
}


} // namespace gui
