#pragma once

#include "karma/ecs/components.hpp"
#include "karma/ecs/world.hpp"
#include "karma/audio/audio.hpp"
#include <optional>
#include <unordered_map>

namespace ecs {

class AudioSyncSystem {
public:
    void update(World &world, Audio *audio) {
#ifdef KARMA_SERVER
        (void)world;
        (void)audio;
        return;
#else
        if (!audio) {
            return;
        }

        const auto &transforms = world.all<Transform>();
        const auto &sources = world.all<AudioSourceComponent>();

        for (auto it = sources_.begin(); it != sources_.end(); ) {
            if (sources.find(it->first) == sources.end()) {
                it = sources_.erase(it);
            } else {
                ++it;
            }
        }

        for (const auto &pair : world.all<AudioListenerComponent>()) {
            const EntityId entity = pair.first;
            const AudioListenerComponent &listener = pair.second;
            if (!listener.active) {
                continue;
            }
            const auto transformIt = transforms.find(entity);
            if (transformIt == transforms.end()) {
                continue;
            }
            audio->setListenerPosition(transformIt->second.position);
            audio->setListenerRotation(transformIt->second.rotation);
            break;
        }

        for (const auto &pair : sources) {
            const EntityId entity = pair.first;
            const AudioSourceComponent &source = pair.second;
            if (source.clip_key.empty()) {
                continue;
            }
            auto &state = sources_[entity];
            if (!state.clip.has_value()) {
                state.clip = audio->loadClip(source.clip_key);
            }
            if (source.play_on_start && !state.started && state.clip.has_value()) {
                glm::vec3 position{0.0f};
                const auto transformIt = transforms.find(entity);
                if (transformIt != transforms.end()) {
                    position = transformIt->second.position;
                }
                state.clip->play(position, source.gain);
                state.started = true;
            }
        }
#endif
    }

private:
    struct SourceState {
        std::optional<AudioClip> clip{};
        bool started = false;
    };
    std::unordered_map<EntityId, SourceState> sources_;
};

} // namespace ecs
