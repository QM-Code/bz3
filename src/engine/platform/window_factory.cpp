#include "platform/window.hpp"

namespace platform {

std::unique_ptr<Window> CreateWindow(const WindowConfig &config) {
#if defined(BZ3_WINDOW_BACKEND_SDL3)
    return CreateSdl3Window(config);
#elif defined(BZ3_WINDOW_BACKEND_SDL2)
    return CreateSdl2Window(config);
#else
    return nullptr;
#endif
}

} // namespace platform
