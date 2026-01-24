#pragma once

#include "audio/backend.hpp"
#include <mutex>
#include <vector>

namespace audio_backend {

class SdlAudioClip final : public Clip {
public:
    SdlAudioClip(std::vector<float> samples, int channels, int maxInstances);
    ~SdlAudioClip() override = default;

    void play(const glm::vec3& position, float volume) override;
    void mix(float* output, int frames, int channels);

private:
    struct Instance {
        int frameOffset = 0;
        float volume = 1.0f;
    };

    std::mutex mutex_;
    std::vector<float> samples_;
    std::vector<Instance> instances_;
    int channels_ = 2;
    int maxInstances_ = 1;
};

} // namespace audio_backend
