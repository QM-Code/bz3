#include "renderer/render.hpp"

#include "renderer/backend.hpp"
#include "spdlog/spdlog.h"

Render::Render(platform::Window& window) {
    backend_ = render_backend::CreateRenderBackend(window);
    if (!backend_) {
        spdlog::error("Render: Failed to create render backend");
    }
}

Render::~Render() = default;

void Render::update() {
    if (backend_) {
        backend_->update();
    }
}

void Render::resizeCallback(int width, int height) {
    if (backend_) {
        backend_->resizeCallback(width, height);
    }
}

render_id Render::create() {
    return backend_ ? backend_->create() : 0;
}

render_id Render::create(std::string modelPath, bool addToRadar) {
    return backend_ ? backend_->create(std::move(modelPath), addToRadar) : 0;
}

void Render::setModel(render_id id, const std::filesystem::path& modelPath, bool addToRadar) {
    if (backend_) {
        backend_->setModel(id, modelPath, addToRadar);
    }
}

void Render::setRadarCircleGraphic(render_id id, float radius) {
    if (backend_) {
        backend_->setRadarCircleGraphic(id, radius);
    }
}

void Render::setRadarFOVLinesAngle(float fovDegrees) {
    if (backend_) {
        backend_->setRadarFOVLinesAngle(fovDegrees);
    }
}

void Render::destroy(render_id id) {
    if (backend_) {
        backend_->destroy(id);
    }
}

void Render::setPosition(render_id id, const glm::vec3& position) {
    if (backend_) {
        backend_->setPosition(id, position);
    }
}

void Render::setRotation(render_id id, const glm::quat& rotation) {
    if (backend_) {
        backend_->setRotation(id, rotation);
    }
}

void Render::setScale(render_id id, const glm::vec3& scale) {
    if (backend_) {
        backend_->setScale(id, scale);
    }
}

void Render::setVisible(render_id id, bool visible) {
    if (backend_) {
        backend_->setVisible(id, visible);
    }
}

void Render::setTransparency(render_id id, bool transparency) {
    if (backend_) {
        backend_->setTransparency(id, transparency);
    }
}

void Render::setCameraPosition(const glm::vec3& position) {
    if (backend_) {
        backend_->setCameraPosition(position);
    }
}

void Render::setCameraRotation(const glm::quat& rotation) {
    if (backend_) {
        backend_->setCameraRotation(rotation);
    }
}

unsigned int Render::getRadarTextureId() const {
    return backend_ ? backend_->getRadarTextureId() : 0;
}

void Render::setRadarShaderPath(const std::filesystem::path& vertPath,
                                const std::filesystem::path& fragPath) {
    if (backend_) {
        backend_->setRadarShaderPath(vertPath, fragPath);
    }
}

glm::mat4 Render::getViewProjectionMatrix() const {
    return backend_ ? backend_->getViewProjectionMatrix() : glm::mat4(1.0f);
}

glm::mat4 Render::getViewMatrix() const {
    return backend_ ? backend_->getViewMatrix() : glm::mat4(1.0f);
}

glm::mat4 Render::getProjectionMatrix() const {
    return backend_ ? backend_->getProjectionMatrix() : glm::mat4(1.0f);
}

glm::vec3 Render::getCameraPosition() const {
    return backend_ ? backend_->getCameraPosition() : glm::vec3(0.0f);
}

glm::vec3 Render::getCameraForward() const {
    return backend_ ? backend_->getCameraForward() : glm::vec3(0.0f, 0.0f, -1.0f);
}
