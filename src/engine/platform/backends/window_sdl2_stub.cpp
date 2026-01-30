#include "platform/window.hpp"

#include <spdlog/spdlog.h>

namespace platform {

std::unique_ptr<Window> CreateSdl2Window(const WindowConfig&) {
    spdlog::error("SDL2 window backend is a stub; SDL2 support is not implemented.");
    return nullptr;
}

} // namespace platform
