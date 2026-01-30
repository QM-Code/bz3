#pragma once

#include "platform/events.hpp"

namespace platform {
class Window;
}

#if defined(BZ3_UI_BACKEND_IMGUI)
#include <imgui.h>
#endif

#if defined(BZ3_UI_BACKEND_RMLUI)
#include <RmlUi/Core/Input.h>
#endif

namespace ui::input_mapping {

#if defined(BZ3_UI_BACKEND_IMGUI)
ImGuiKey ToImGuiKey(platform::Key key);
int ToImGuiMouseButton(platform::MouseButton button);
void UpdateImGuiModifiers(ImGuiIO &io, platform::Window *window);
#endif

#if defined(BZ3_UI_BACKEND_RMLUI)
Rml::Input::KeyIdentifier ToRmlKey(platform::Key key);
int ToRmlMouseButton(platform::MouseButton button);
int ToRmlMods(const platform::Modifiers &mods);
int CurrentRmlMods(platform::Window *window);
int RmlModsForEvent(const platform::Event &event, platform::Window *window);
#endif

} // namespace ui::input_mapping
