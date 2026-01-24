#include "audio/audio.hpp"

#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>
#include "spdlog/spdlog.h"

#include <algorithm>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
std::string buildCacheKey(const std::string& filepath, int maxInstances) {
    return filepath + "#" + std::to_string(maxInstances);
}
}

class AudioClipData {
public:
    AudioClipData(ma_sound* stem, std::vector<ma_sound*> instances)
        : stem_(stem), instances_(std::move(instances)) {}

    ~AudioClipData() {
        release();
    }

    void play(const glm::vec3& position, float volume) {
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

    void release() {
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

private:
    ma_sound* stem_ = nullptr;
    std::vector<ma_sound*> instances_;
    bool released_ = false;
};

Audio::Audio() {
    engine = new ma_engine();
    const ma_result result = ma_engine_init(nullptr, engine);
    if (result != MA_SUCCESS) {
        delete engine;
        engine = nullptr;
        throw std::runtime_error("Audio: Failed to initialize miniaudio engine");
    }

    spdlog::trace("Audio: Initialized miniaudio engine");
}

Audio::~Audio() {
    for (auto& [key, weakClip] : clipCache_) {
        if (auto clip = weakClip.lock()) {
            clip->release();
        }
    }
    clipCache_.clear();

    if (engine != nullptr) {
        ma_engine_uninit(engine);
        delete engine;
        engine = nullptr;
    }
}

std::shared_ptr<AudioClipData> Audio::createClip(const std::string& filepath, int maxInstances) {
    if (engine == nullptr) {
        throw std::runtime_error("Audio: Engine not initialized");
    }

    auto stem = std::make_unique<ma_sound>();
    if (ma_sound_init_from_file(engine, filepath.c_str(), 0, nullptr, nullptr, stem.get()) != MA_SUCCESS) {
        spdlog::error("Audio: Failed to load audio file '{}'", filepath);
        throw std::runtime_error("Audio: Failed to load audio file");
    }

    std::vector<ma_sound*> instances;
    instances.reserve(std::max(1, maxInstances));

    for (int i = 0; i < maxInstances; ++i) {
        auto pooledSound = std::make_unique<ma_sound>();
        if (ma_sound_init_from_file(engine, filepath.c_str(), 0, nullptr, nullptr, pooledSound.get()) != MA_SUCCESS) {
            spdlog::error("Audio: Failed to create pooled instance {} for '{}'", i, filepath);
            continue;
        }

        ma_sound_set_looping(pooledSound.get(), MA_FALSE);
        ma_sound_stop(pooledSound.get());
        ma_sound_seek_to_pcm_frame(pooledSound.get(), 0);
        instances.push_back(pooledSound.release());
    }

    if (instances.empty()) {
        ma_sound_uninit(stem.get());
        spdlog::error("Audio: Unable to create playable instances for '{}'", filepath);
        throw std::runtime_error("Audio: Clip has no playable instances");
    }

    auto clipData = std::make_shared<AudioClipData>(stem.release(), std::move(instances));
    return clipData;
}

AudioClip Audio::loadClip(const std::string& filepath, int maxInstances) {
    const std::string cacheKey = buildCacheKey(filepath, maxInstances);

    if (auto it = clipCache_.find(cacheKey); it != clipCache_.end()) {
        if (auto cached = it->second.lock()) {
            return AudioClip(std::move(cached));
        }
    }

    auto clipData = createClip(filepath, maxInstances);
    clipCache_[cacheKey] = clipData;
    return AudioClip(std::move(clipData));
}

void Audio::setListenerPosition(const glm::vec3 &position) {
    ma_engine_listener_set_position(engine, 0, position.x, position.y, position.z);
}

void Audio::setListenerRotation(const glm::quat &rotation) {
    const glm::vec3 forward = rotation * glm::vec3(0.0f, 0.0f, -1.0f);
    ma_engine_listener_set_direction(engine, 0, forward.x, forward.y, forward.z);
}

AudioClip::AudioClip(std::shared_ptr<AudioClipData> data)
    : data_(std::move(data)) {}

void AudioClip::play(const glm::vec3& position, float volume) const {
    if (!data_) {
        spdlog::error("AudioClip: Attempted to play an uninitialized clip");
        return;
    }

    data_->play(position, volume);
}

