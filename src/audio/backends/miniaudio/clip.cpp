#include "audio/backends/miniaudio/clip.hpp"
#include "spdlog/spdlog.h"

namespace audio_backend {

MiniaudioClip::MiniaudioClip(ma_sound* stem, std::vector<ma_sound*> instances)
    : stem_(stem), instances_(std::move(instances)) {}

MiniaudioClip::~MiniaudioClip() {
    release();
}

void MiniaudioClip::play(const glm::vec3& position, float volume) {
    if (released_) {
        spdlog::warn("AudioClip: Attempted to play a released clip");
        return;
    }

    ma_sound* soundToPlay = nullptr;
    for (auto* sound : instances_) {
        if (!ma_sound_is_playing(sound)) {
            soundToPlay = sound;
            break;
        }
    }

    if (soundToPlay == nullptr) {
        spdlog::warn("AudioClip: No available sound instances");
        return;
    }

    ma_sound_stop(soundToPlay);
    ma_sound_seek_to_pcm_frame(soundToPlay, 0);
    ma_sound_set_position(soundToPlay, position.x, position.y, position.z);
    ma_sound_set_volume(soundToPlay, volume);
    ma_sound_start(soundToPlay);
}

void MiniaudioClip::release() {
    if (released_) {
        return;
    }

    for (auto* sound : instances_) {
        ma_sound_uninit(sound);
        delete sound;
    }
    instances_.clear();

    if (stem_ != nullptr) {
        ma_sound_uninit(stem_);
        delete stem_;
        stem_ = nullptr;
    }

    released_ = true;
}

} // namespace audio_backend
