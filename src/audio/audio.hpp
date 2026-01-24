#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <memory>
#include <string>
#include <unordered_map>

struct ma_engine;

class AudioClipData;
class ClientEngine;

class AudioClip {
public:
    AudioClip() = delete;
    AudioClip(const AudioClip&) = default;
    AudioClip(AudioClip&&) noexcept = default;
    AudioClip& operator=(const AudioClip&) = default;
    AudioClip& operator=(AudioClip&&) noexcept = default;
    ~AudioClip() = default;

    void play(const glm::vec3& position, float volume = 1.0f) const;

private:
    friend class Audio;
    explicit AudioClip(std::shared_ptr<AudioClipData> data);

    std::shared_ptr<AudioClipData> data_;
};

class Audio {
    friend class ClientEngine;

public:
    Audio();
    ~Audio();

    AudioClip loadClip(const std::string& filepath, int maxInstances = 5);
    void setListenerPosition(const glm::vec3& position);
    void setListenerRotation(const glm::quat& rotation);

private:
    std::shared_ptr<AudioClipData> createClip(const std::string& filepath, int maxInstances);

    ma_engine* engine = nullptr;
    std::unordered_map<std::string, std::weak_ptr<AudioClipData>> clipCache_;
};
