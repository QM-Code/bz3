#include "ui/config/input_mapping.hpp"

#include "platform/window.hpp"

namespace ui::input_mapping {

#if defined(BZ3_UI_BACKEND_IMGUI)
ImGuiKey ToImGuiKey(platform::Key key) {
    switch (key) {
        case platform::Key::A: return ImGuiKey_A;
        case platform::Key::B: return ImGuiKey_B;
        case platform::Key::C: return ImGuiKey_C;
        case platform::Key::D: return ImGuiKey_D;
        case platform::Key::E: return ImGuiKey_E;
        case platform::Key::F: return ImGuiKey_F;
        case platform::Key::G: return ImGuiKey_G;
        case platform::Key::H: return ImGuiKey_H;
        case platform::Key::I: return ImGuiKey_I;
        case platform::Key::J: return ImGuiKey_J;
        case platform::Key::K: return ImGuiKey_K;
        case platform::Key::L: return ImGuiKey_L;
        case platform::Key::M: return ImGuiKey_M;
        case platform::Key::N: return ImGuiKey_N;
        case platform::Key::O: return ImGuiKey_O;
        case platform::Key::P: return ImGuiKey_P;
        case platform::Key::Q: return ImGuiKey_Q;
        case platform::Key::R: return ImGuiKey_R;
        case platform::Key::S: return ImGuiKey_S;
        case platform::Key::T: return ImGuiKey_T;
        case platform::Key::U: return ImGuiKey_U;
        case platform::Key::V: return ImGuiKey_V;
        case platform::Key::W: return ImGuiKey_W;
        case platform::Key::X: return ImGuiKey_X;
        case platform::Key::Y: return ImGuiKey_Y;
        case platform::Key::Z: return ImGuiKey_Z;
        case platform::Key::Num0: return ImGuiKey_0;
        case platform::Key::Num1: return ImGuiKey_1;
        case platform::Key::Num2: return ImGuiKey_2;
        case platform::Key::Num3: return ImGuiKey_3;
        case platform::Key::Num4: return ImGuiKey_4;
        case platform::Key::Num5: return ImGuiKey_5;
        case platform::Key::Num6: return ImGuiKey_6;
        case platform::Key::Num7: return ImGuiKey_7;
        case platform::Key::Num8: return ImGuiKey_8;
        case platform::Key::Num9: return ImGuiKey_9;
        case platform::Key::F1: return ImGuiKey_F1;
        case platform::Key::F2: return ImGuiKey_F2;
        case platform::Key::F3: return ImGuiKey_F3;
        case platform::Key::F4: return ImGuiKey_F4;
        case platform::Key::F5: return ImGuiKey_F5;
        case platform::Key::F6: return ImGuiKey_F6;
        case platform::Key::F7: return ImGuiKey_F7;
        case platform::Key::F8: return ImGuiKey_F8;
        case platform::Key::F9: return ImGuiKey_F9;
        case platform::Key::F10: return ImGuiKey_F10;
        case platform::Key::F11: return ImGuiKey_F11;
        case platform::Key::F12: return ImGuiKey_F12;
        case platform::Key::F13: return ImGuiKey_F13;
        case platform::Key::F14: return ImGuiKey_F14;
        case platform::Key::F15: return ImGuiKey_F15;
        case platform::Key::F16: return ImGuiKey_F16;
        case platform::Key::F17: return ImGuiKey_F17;
        case platform::Key::F18: return ImGuiKey_F18;
        case platform::Key::F19: return ImGuiKey_F19;
        case platform::Key::F20: return ImGuiKey_F20;
        case platform::Key::F21: return ImGuiKey_F21;
        case platform::Key::F22: return ImGuiKey_F22;
        case platform::Key::F23: return ImGuiKey_F23;
        case platform::Key::F24: return ImGuiKey_F24;
        case platform::Key::Space: return ImGuiKey_Space;
        case platform::Key::Escape: return ImGuiKey_Escape;
        case platform::Key::Enter: return ImGuiKey_Enter;
        case platform::Key::Tab: return ImGuiKey_Tab;
        case platform::Key::Backspace: return ImGuiKey_Backspace;
        case platform::Key::Left: return ImGuiKey_LeftArrow;
        case platform::Key::Right: return ImGuiKey_RightArrow;
        case platform::Key::Up: return ImGuiKey_UpArrow;
        case platform::Key::Down: return ImGuiKey_DownArrow;
        case platform::Key::LeftBracket: return ImGuiKey_LeftBracket;
        case platform::Key::RightBracket: return ImGuiKey_RightBracket;
        case platform::Key::Minus: return ImGuiKey_Minus;
        case platform::Key::Equal: return ImGuiKey_Equal;
        case platform::Key::Apostrophe: return ImGuiKey_Apostrophe;
        case platform::Key::GraveAccent: return ImGuiKey_GraveAccent;
        case platform::Key::LeftShift: return ImGuiKey_LeftShift;
        case platform::Key::RightShift: return ImGuiKey_RightShift;
        case platform::Key::LeftControl: return ImGuiKey_LeftCtrl;
        case platform::Key::RightControl: return ImGuiKey_RightCtrl;
        case platform::Key::LeftAlt: return ImGuiKey_LeftAlt;
        case platform::Key::RightAlt: return ImGuiKey_RightAlt;
        case platform::Key::LeftSuper: return ImGuiKey_LeftSuper;
        case platform::Key::RightSuper: return ImGuiKey_RightSuper;
        case platform::Key::Menu: return ImGuiKey_Menu;
        case platform::Key::Home: return ImGuiKey_Home;
        case platform::Key::End: return ImGuiKey_End;
        case platform::Key::PageUp: return ImGuiKey_PageUp;
        case platform::Key::PageDown: return ImGuiKey_PageDown;
        case platform::Key::Insert: return ImGuiKey_Insert;
        case platform::Key::Delete: return ImGuiKey_Delete;
        default: return ImGuiKey_None;
    }
}

int ToImGuiMouseButton(platform::MouseButton button) {
    switch (button) {
        case platform::MouseButton::Left: return 0;
        case platform::MouseButton::Right: return 1;
        case platform::MouseButton::Middle: return 2;
        case platform::MouseButton::Button4: return 3;
        case platform::MouseButton::Button5: return 4;
        case platform::MouseButton::Button6: return 5;
        case platform::MouseButton::Button7: return 6;
        case platform::MouseButton::Button8: return 7;
        default: return 0;
    }
}

void UpdateImGuiModifiers(ImGuiIO &io, platform::Window *window) {
    if (!window) {
        return;
    }
    const bool shift = window->isKeyDown(platform::Key::LeftShift) || window->isKeyDown(platform::Key::RightShift);
    const bool ctrl = window->isKeyDown(platform::Key::LeftControl) || window->isKeyDown(platform::Key::RightControl);
    const bool alt = window->isKeyDown(platform::Key::LeftAlt) || window->isKeyDown(platform::Key::RightAlt);
    const bool super = window->isKeyDown(platform::Key::LeftSuper) || window->isKeyDown(platform::Key::RightSuper);
    io.AddKeyEvent(ImGuiKey_ModShift, shift);
    io.AddKeyEvent(ImGuiKey_ModCtrl, ctrl);
    io.AddKeyEvent(ImGuiKey_ModAlt, alt);
    io.AddKeyEvent(ImGuiKey_ModSuper, super);
}
#endif

#if defined(BZ3_UI_BACKEND_RMLUI)
Rml::Input::KeyIdentifier ToRmlKey(platform::Key key) {
    switch (key) {
        case platform::Key::A: return Rml::Input::KI_A;
        case platform::Key::B: return Rml::Input::KI_B;
        case platform::Key::C: return Rml::Input::KI_C;
        case platform::Key::D: return Rml::Input::KI_D;
        case platform::Key::E: return Rml::Input::KI_E;
        case platform::Key::F: return Rml::Input::KI_F;
        case platform::Key::G: return Rml::Input::KI_G;
        case platform::Key::H: return Rml::Input::KI_H;
        case platform::Key::I: return Rml::Input::KI_I;
        case platform::Key::J: return Rml::Input::KI_J;
        case platform::Key::K: return Rml::Input::KI_K;
        case platform::Key::L: return Rml::Input::KI_L;
        case platform::Key::M: return Rml::Input::KI_M;
        case platform::Key::N: return Rml::Input::KI_N;
        case platform::Key::O: return Rml::Input::KI_O;
        case platform::Key::P: return Rml::Input::KI_P;
        case platform::Key::Q: return Rml::Input::KI_Q;
        case platform::Key::R: return Rml::Input::KI_R;
        case platform::Key::S: return Rml::Input::KI_S;
        case platform::Key::T: return Rml::Input::KI_T;
        case platform::Key::U: return Rml::Input::KI_U;
        case platform::Key::V: return Rml::Input::KI_V;
        case platform::Key::W: return Rml::Input::KI_W;
        case platform::Key::X: return Rml::Input::KI_X;
        case platform::Key::Y: return Rml::Input::KI_Y;
        case platform::Key::Z: return Rml::Input::KI_Z;
        case platform::Key::Num0: return Rml::Input::KI_0;
        case platform::Key::Num1: return Rml::Input::KI_1;
        case platform::Key::Num2: return Rml::Input::KI_2;
        case platform::Key::Num3: return Rml::Input::KI_3;
        case platform::Key::Num4: return Rml::Input::KI_4;
        case platform::Key::Num5: return Rml::Input::KI_5;
        case platform::Key::Num6: return Rml::Input::KI_6;
        case platform::Key::Num7: return Rml::Input::KI_7;
        case platform::Key::Num8: return Rml::Input::KI_8;
        case platform::Key::Num9: return Rml::Input::KI_9;
        case platform::Key::F1: return Rml::Input::KI_F1;
        case platform::Key::F2: return Rml::Input::KI_F2;
        case platform::Key::F3: return Rml::Input::KI_F3;
        case platform::Key::F4: return Rml::Input::KI_F4;
        case platform::Key::F5: return Rml::Input::KI_F5;
        case platform::Key::F6: return Rml::Input::KI_F6;
        case platform::Key::F7: return Rml::Input::KI_F7;
        case platform::Key::F8: return Rml::Input::KI_F8;
        case platform::Key::F9: return Rml::Input::KI_F9;
        case platform::Key::F10: return Rml::Input::KI_F10;
        case platform::Key::F11: return Rml::Input::KI_F11;
        case platform::Key::F12: return Rml::Input::KI_F12;
        case platform::Key::F13: return Rml::Input::KI_F13;
        case platform::Key::F14: return Rml::Input::KI_F14;
        case platform::Key::F15: return Rml::Input::KI_F15;
        case platform::Key::F16: return Rml::Input::KI_F16;
        case platform::Key::F17: return Rml::Input::KI_F17;
        case platform::Key::F18: return Rml::Input::KI_F18;
        case platform::Key::F19: return Rml::Input::KI_F19;
        case platform::Key::F20: return Rml::Input::KI_F20;
        case platform::Key::F21: return Rml::Input::KI_F21;
        case platform::Key::F22: return Rml::Input::KI_F22;
        case platform::Key::F23: return Rml::Input::KI_F23;
        case platform::Key::F24: return Rml::Input::KI_F24;
        case platform::Key::Space: return Rml::Input::KI_SPACE;
        case platform::Key::Escape: return Rml::Input::KI_ESCAPE;
        case platform::Key::Enter: return Rml::Input::KI_RETURN;
        case platform::Key::Tab: return Rml::Input::KI_TAB;
        case platform::Key::Backspace: return Rml::Input::KI_BACK;
        case platform::Key::Left: return Rml::Input::KI_LEFT;
        case platform::Key::Right: return Rml::Input::KI_RIGHT;
        case platform::Key::Up: return Rml::Input::KI_UP;
        case platform::Key::Down: return Rml::Input::KI_DOWN;
        case platform::Key::LeftBracket: return Rml::Input::KI_OEM_4;
        case platform::Key::RightBracket: return Rml::Input::KI_OEM_6;
        case platform::Key::Minus: return Rml::Input::KI_OEM_MINUS;
        case platform::Key::Equal: return Rml::Input::KI_OEM_PLUS;
        case platform::Key::Apostrophe: return Rml::Input::KI_OEM_7;
        case platform::Key::GraveAccent: return Rml::Input::KI_OEM_3;
        case platform::Key::LeftShift: return Rml::Input::KI_LSHIFT;
        case platform::Key::RightShift: return Rml::Input::KI_RSHIFT;
        case platform::Key::LeftControl: return Rml::Input::KI_LCONTROL;
        case platform::Key::RightControl: return Rml::Input::KI_RCONTROL;
        case platform::Key::LeftAlt: return Rml::Input::KI_LMENU;
        case platform::Key::RightAlt: return Rml::Input::KI_RMENU;
        case platform::Key::LeftSuper: return Rml::Input::KI_LMETA;
        case platform::Key::RightSuper: return Rml::Input::KI_RMETA;
        case platform::Key::Home: return Rml::Input::KI_HOME;
        case platform::Key::End: return Rml::Input::KI_END;
        case platform::Key::PageUp: return Rml::Input::KI_PRIOR;
        case platform::Key::PageDown: return Rml::Input::KI_NEXT;
        case platform::Key::Insert: return Rml::Input::KI_INSERT;
        case platform::Key::Delete: return Rml::Input::KI_DELETE;
        case platform::Key::CapsLock: return Rml::Input::KI_CAPITAL;
        case platform::Key::NumLock: return Rml::Input::KI_NUMLOCK;
        case platform::Key::ScrollLock: return Rml::Input::KI_SCROLL;
        default: return Rml::Input::KI_UNKNOWN;
    }
}

int ToRmlMods(const platform::Modifiers &mods) {
    int out = 0;
    if (mods.control) {
        out |= Rml::Input::KM_CTRL;
    }
    if (mods.shift) {
        out |= Rml::Input::KM_SHIFT;
    }
    if (mods.alt) {
        out |= Rml::Input::KM_ALT;
    }
    if (mods.super) {
        out |= Rml::Input::KM_META;
    }
    return out;
}

int CurrentRmlMods(platform::Window *window) {
    if (!window) {
        return 0;
    }
    platform::Modifiers mods;
    mods.shift = window->isKeyDown(platform::Key::LeftShift) || window->isKeyDown(platform::Key::RightShift);
    mods.control = window->isKeyDown(platform::Key::LeftControl) || window->isKeyDown(platform::Key::RightControl);
    mods.alt = window->isKeyDown(platform::Key::LeftAlt) || window->isKeyDown(platform::Key::RightAlt);
    mods.super = window->isKeyDown(platform::Key::LeftSuper) || window->isKeyDown(platform::Key::RightSuper);
    return ToRmlMods(mods);
}

int RmlModsForEvent(const platform::Event &event, platform::Window *window) {
    const int mods = ToRmlMods(event.mods);
    if (mods == 0) {
        return CurrentRmlMods(window);
    }
    return mods;
}

int ToRmlMouseButton(platform::MouseButton button) {
    switch (button) {
        case platform::MouseButton::Left: return 0;
        case platform::MouseButton::Right: return 1;
        case platform::MouseButton::Middle: return 2;
        case platform::MouseButton::Button4: return 3;
        case platform::MouseButton::Button5: return 4;
        case platform::MouseButton::Button6: return 5;
        case platform::MouseButton::Button7: return 6;
        case platform::MouseButton::Button8: return 7;
        default: return 0;
    }
}
#endif

} // namespace ui::input_mapping
