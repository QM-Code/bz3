#pragma once

#include <vector>

struct ImGuiIO;

#include "ui/core/types.hpp"

namespace ui {

class ImGuiHudScoreboard {
public:
    void setEntries(const std::vector<ScoreboardEntry> &entriesIn);
    void draw(ImGuiIO &io);

private:
    std::vector<ScoreboardEntry> entries;
};

} // namespace ui
