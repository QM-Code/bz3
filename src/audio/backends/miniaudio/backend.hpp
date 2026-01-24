#pragma once

#include "audio/backend.hpp"
#include <memory>

struct ma_engine;

namespace audio_backend {

class MiniaudioBackend final : public Backend {
public:
    MiniaudioBackend();
    ~MiniaudioBackend() override;

    std::shared_ptr<Clip> loadClip(const std::string& filepath, const ClipOptions& options) override;
    void setListenerPosition(const glm::vec3& position) override;
    void setListenerRotation(const glm::quat& rotation) override;

private:
    ma_engine* engine_ = nullptr;
};

} // namespace audio_backend
