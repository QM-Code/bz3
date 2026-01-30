#include "ui/frontends/imgui/console/console.hpp"

namespace ui {

void ConsoleView::drawDocumentationPanel(const MessageColors &colors) const {
    drawPlaceholderPanel(
        "?",
        "This space intentionally left blank.",
        colors);
}


} // namespace ui
