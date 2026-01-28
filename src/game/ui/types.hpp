#pragma once

#include <string>

#include "engine/graphics/texture_handle.hpp"

struct ScoreboardEntry {
    std::string name;
    int score;
    bool registeredUser = false;
    bool communityAdmin = false;
    bool localAdmin = false;
};

namespace ui {

struct RenderOutput {
    graphics::TextureHandle texture{};
    bool visible = true;

    bool valid() const {
        return visible && texture.valid();
    }
};

} // namespace ui
