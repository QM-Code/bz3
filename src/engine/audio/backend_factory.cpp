#include "audio/backend.hpp"

#if defined(KARMA_AUDIO_BACKEND_MINIAUDIO)
#include "audio/backends/miniaudio/backend.hpp"
#elif defined(KARMA_AUDIO_BACKEND_SDL)
#include "audio/backends/sdl/backend.hpp"
#else
#error "KARMA audio backend not set. Define KARMA_AUDIO_BACKEND_MINIAUDIO or KARMA_AUDIO_BACKEND_SDL."
#endif

namespace audio_backend {

std::unique_ptr<Backend> CreateAudioBackend() {
#if defined(KARMA_AUDIO_BACKEND_MINIAUDIO)
    return std::make_unique<MiniaudioBackend>();
#elif defined(KARMA_AUDIO_BACKEND_SDL)
    return std::make_unique<SdlAudioBackend>();
#endif
}

} // namespace audio_backend
