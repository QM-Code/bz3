#include "engine/components/gui/main_menu.hpp"

namespace gui {

void MainMenuView::drawSettingsPanel(const MessageColors &colors) const {
    drawPlaceholderPanel(
        "Settings",
        "Manage client preferences (keybindings, LAN visibility, fullscreen, and more).",
        colors);
}


} // namespace gui
