#pragma once

#include <vector>

struct ImGuiIO;
struct ImVec4;

#include "ui/core/types.hpp"

namespace ui {

class ImGuiHudScoreboard {
public:
    void setEntries(const std::vector<ScoreboardEntry> &entriesIn);
    void draw(ImGuiIO &io, const ImVec4 &backgroundColor);

private:
    std::vector<ScoreboardEntry> entries;
};

} // namespace ui
