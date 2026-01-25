#include "audio/backends/sdl/backend.hpp"
#include "audio/backends/sdl/clip.hpp"
#include "spdlog/spdlog.h"

#include <SDL3/SDL.h>
#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace {
constexpr int kDefaultFrequency = 48000;
constexpr int kDefaultChannels = 2;
}

namespace audio_backend {

SdlAudioBackend::SdlAudioBackend() {
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        throw std::runtime_error("Audio: SDL audio subsystem failed to initialize");
    }

    deviceSpec_.format = SDL_AUDIO_F32;
    deviceSpec_.channels = kDefaultChannels;
    deviceSpec_.freq = kDefaultFrequency;

    stream_ = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
                                        &deviceSpec_,
                                        AudioStreamCallback,
                                        this);
    if (!stream_) {
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        throw std::runtime_error("Audio: Failed to open SDL audio device");
    }

    SDL_ResumeAudioStreamDevice(stream_);
}

SdlAudioBackend::~SdlAudioBackend() {
    if (stream_) {
        SDL_DestroyAudioStream(stream_);
        stream_ = nullptr;
    }
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

std::shared_ptr<Clip> SdlAudioBackend::loadClip(const std::string& filepath, const ClipOptions& options) {
    SDL_AudioSpec srcSpec{};
    Uint8* srcBuffer = nullptr;
    Uint32 srcLength = 0;
    if (!SDL_LoadWAV(filepath.c_str(), &srcSpec, &srcBuffer, &srcLength)) {
        spdlog::error("Audio: Failed to load WAV '{}': {}", filepath, SDL_GetError());
        throw std::runtime_error("Audio: Failed to load WAV");
    }

    Uint8* dstBuffer = nullptr;
    int dstLength = 0;
    if (!SDL_ConvertAudioSamples(&srcSpec,
                                 srcBuffer,
                                 static_cast<int>(srcLength),
                                 &deviceSpec_,
                                 &dstBuffer,
                                 &dstLength)) {
        SDL_free(srcBuffer);
        spdlog::error("Audio: Failed to convert WAV '{}': {}", filepath, SDL_GetError());
        throw std::runtime_error("Audio: Failed to convert WAV");
    }

    SDL_free(srcBuffer);

    const size_t sampleCount = static_cast<size_t>(dstLength) / sizeof(float);
    std::vector<float> samples(sampleCount);
    std::memcpy(samples.data(), dstBuffer, dstLength);
    SDL_free(dstBuffer);

    auto clip = std::make_shared<SdlAudioClip>(std::move(samples), deviceSpec_.channels, std::max(1, options.maxInstances));
    {
        std::lock_guard<std::mutex> lock(mutex_);
        clips_.push_back(clip);
    }
    return clip;
}

void SdlAudioBackend::setListenerPosition(const glm::vec3&) {}

void SdlAudioBackend::setListenerRotation(const glm::quat&) {}

void SDLCALL SdlAudioBackend::AudioStreamCallback(void* userdata, SDL_AudioStream* stream, int additional_amount, int) {
    if (additional_amount <= 0 || !userdata) {
        return;
    }

    auto* backend = static_cast<SdlAudioBackend*>(userdata);
    const int frames = additional_amount / (sizeof(float) * backend->deviceSpec_.channels);
    if (frames <= 0) {
        return;
    }

    std::vector<float> buffer(static_cast<size_t>(frames * backend->deviceSpec_.channels), 0.0f);
    backend->mixAudio(buffer.data(), frames);
    SDL_PutAudioStreamData(stream, buffer.data(), additional_amount);
}

void SdlAudioBackend::mixAudio(float* output, int frames) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto it = clips_.begin(); it != clips_.end();) {
        if (auto clip = it->lock()) {
            clip->mix(output, frames, deviceSpec_.channels);
            ++it;
        } else {
            it = clips_.erase(it);
        }
    }
}

} // namespace audio_backend
