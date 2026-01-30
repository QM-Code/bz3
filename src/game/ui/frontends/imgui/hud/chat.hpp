#pragma once

#include <array>
#include <string>
#include <vector>

struct ImGuiIO;
struct ImVec2;

namespace ui {

class ImGuiHudChat {
public:
    void addLine(const std::string &playerName, const std::string &line);
    void setLines(const std::vector<std::string> &lines);
    std::string getSubmittedInput() const;
    void clearSubmittedInput();
    void focusInput();
    void clearFocus();
    bool isFocused() const;

    void draw(const ImVec2 &pos, const ImVec2 &size, float inputHeight);

private:
    std::vector<std::string> consoleLines;
    std::array<char, 256> chatInputBuffer{};
    std::string submittedInputBuffer;
    bool chatFocus = false;
    bool autoScroll = true;
};

} // namespace ui
