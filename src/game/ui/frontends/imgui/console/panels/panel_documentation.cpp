#include "ui/frontends/imgui/console/console.hpp"

namespace ui {

void ConsoleView::drawDocumentationPanel(const MessageColors &colors) const {
    drawPlaceholderPanel(
        "Documentation",
        "Browse manuals, onboarding tips, and gameplay references.",
        colors);
}


} // namespace ui
