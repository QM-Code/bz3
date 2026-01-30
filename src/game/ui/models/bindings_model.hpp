#pragma once

#include <array>
#include <cstdint>
#include <string>

namespace ui {

struct BindingsModel {
    enum class Column {
        Keyboard,
        Mouse,
        Controller
    };

    static constexpr std::size_t kKeybindingCount = 11;

    std::array<std::array<char, 128>, kKeybindingCount> keyboard{};
    std::array<std::array<char, 128>, kKeybindingCount> mouse{};
    std::array<std::array<char, 128>, kKeybindingCount> controller{};
    int selectedIndex = -1;
    Column selectedColumn = Column::Keyboard;
    bool loaded = false;
    bool statusIsError = false;
    std::string statusText;
    bool keybindingsReloadRequested = false;
};

} // namespace ui
