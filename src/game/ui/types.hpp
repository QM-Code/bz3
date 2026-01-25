#pragma once

#include <string>

struct ScoreboardEntry {
    std::string name;
    int score;
    bool registeredUser = false;
    bool communityAdmin = false;
    bool localAdmin = false;
};

namespace ui {

struct RenderOutput {
    unsigned int textureId = 0;
    int width = 0;
    int height = 0;

    bool valid() const {
        return textureId != 0 && width > 0 && height > 0;
    }
};

} // namespace ui
