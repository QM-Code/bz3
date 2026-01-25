#pragma once

#include <glm/glm.hpp>
#include <string>

class Input;

namespace game_input {

struct InputState {
    bool fire = false;
    bool spawn = false;
    bool jump = false;
    bool quickQuit = false;
    bool chat = false;
    bool escape = false;
    bool toggleFullscreen = false;
    glm::vec2 movement{0.0f, 0.0f};
};

InputState BuildInputState(const Input& input);
std::string SpawnHintText(const Input& input);

} // namespace game_input
