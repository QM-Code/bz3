#include "audio/audio.hpp"
#include "audio/backend.hpp"
#include "spdlog/spdlog.h"

#include <algorithm>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

namespace {
std::string buildCacheKey(const std::string& filepath, int maxInstances) {
    return filepath + "#" + std::to_string(maxInstances);
}
}

Audio::Audio() {
    backend_ = audio_backend::CreateAudioBackend();
}

Audio::~Audio() {
    clipCache_.clear();
}

std::shared_ptr<audio_backend::Clip> Audio::createClip(const std::string& filepath, int maxInstances) {
    if (!backend_) {
        throw std::runtime_error("Audio: Backend not initialized");
    }

    audio_backend::ClipOptions options;
    options.maxInstances = maxInstances;
    return backend_->loadClip(filepath, options);
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

void Audio::setListenerPosition(const glm::vec3& position) {
    if (backend_) {
        backend_->setListenerPosition(position);
    }
}

void Audio::setListenerRotation(const glm::quat& rotation) {
    if (backend_) {
        backend_->setListenerRotation(rotation);
    }
}

AudioClip::AudioClip(std::shared_ptr<audio_backend::Clip> data)
    : data_(std::move(data)) {}

void AudioClip::play(const glm::vec3& position, float volume) const {
    if (!data_) {
        spdlog::error("AudioClip: Attempted to play an uninitialized clip");
        return;
    }

    data_->play(position, volume);
}
