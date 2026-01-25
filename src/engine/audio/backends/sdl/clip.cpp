#include "audio/backends/sdl/clip.hpp"
#include "spdlog/spdlog.h"

namespace audio_backend {

SdlAudioClip::SdlAudioClip(std::vector<float> samples, int channels, int maxInstances)
    : samples_(std::move(samples)), channels_(channels), maxInstances_(maxInstances) {}

void SdlAudioClip::play(const glm::vec3&, float volume) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (static_cast<int>(instances_.size()) >= maxInstances_) {
        spdlog::warn("AudioClip: No available sound instances");
        return;
    }

    Instance instance;
    instance.frameOffset = 0;
    instance.volume = volume;
    instances_.push_back(instance);
}

void SdlAudioClip::mix(float* output, int frames, int channels) {
    if (channels_ != channels) {
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    if (instances_.empty() || samples_.empty()) {
        return;
    }

    const int totalFrames = static_cast<int>(samples_.size() / channels_);
    for (auto it = instances_.begin(); it != instances_.end();) {
        int offset = it->frameOffset;
        const float volume = it->volume;
        if (offset >= totalFrames) {
            it = instances_.erase(it);
            continue;
        }

        const int framesToMix = std::min(frames, totalFrames - offset);
        const int startIndex = offset * channels_;
        for (int frame = 0; frame < framesToMix; ++frame) {
            const int sampleIndex = startIndex + frame * channels_;
            const int outIndex = frame * channels_;
            for (int ch = 0; ch < channels_; ++ch) {
                output[outIndex + ch] += samples_[sampleIndex + ch] * volume;
            }
        }

        it->frameOffset += framesToMix;
        if (it->frameOffset >= totalFrames) {
            it = instances_.erase(it);
        } else {
            ++it;
        }
    }
}

} // namespace audio_backend
