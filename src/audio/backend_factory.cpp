#include "audio/backend.hpp"

#if defined(BZ3_AUDIO_BACKEND_MINIAUDIO)
#include "audio/backends/miniaudio/backend.hpp"
#elif defined(BZ3_AUDIO_BACKEND_SDL)
#include "audio/backends/sdl/backend.hpp"
#else
#error "BZ3 audio backend not set. Define BZ3_AUDIO_BACKEND_MINIAUDIO or BZ3_AUDIO_BACKEND_SDL."
#endif

namespace audio_backend {

std::unique_ptr<Backend> CreateAudioBackend() {
#if defined(BZ3_AUDIO_BACKEND_MINIAUDIO)
    return std::make_unique<MiniaudioBackend>();
#elif defined(BZ3_AUDIO_BACKEND_SDL)
    return std::make_unique<SdlAudioBackend>();
#endif
}

} // namespace audio_backend
