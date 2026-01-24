#pragma once

#include "audio/backend.hpp"
#include <miniaudio.h>
#include <vector>

namespace audio_backend {

class MiniaudioClip final : public Clip {
public:
    MiniaudioClip(ma_sound* stem, std::vector<ma_sound*> instances);
    ~MiniaudioClip() override;

    void play(const glm::vec3& position, float volume) override;

private:
    void release();

    ma_sound* stem_ = nullptr;
    std::vector<ma_sound*> instances_;
    bool released_ = false;
};

} // namespace audio_backend
