#include "ui/backend.hpp"

#if defined(BZ3_UI_BACKEND_IMGUI)
#include "ui/backends/imgui/backend.hpp"
#elif defined(BZ3_UI_BACKEND_RMLUI)
#include "ui/backends/rmlui/backend.hpp"
#else
#error "BZ3 UI backend not set. Define BZ3_UI_BACKEND_IMGUI or BZ3_UI_BACKEND_RMLUI."
#endif

namespace ui_backend {

std::unique_ptr<Backend> CreateUiBackend(platform::Window &window) {
#if defined(BZ3_UI_BACKEND_IMGUI)
    return std::make_unique<ImGuiBackend>(window);
#elif defined(BZ3_UI_BACKEND_RMLUI)
    return std::make_unique<RmlUiBackend>(window);
#endif
}

} // namespace ui_backend
