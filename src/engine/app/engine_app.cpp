#include "karma/app/engine_app.hpp"
#include "karma/core/types.hpp"
#include "karma/renderer/renderer_core.hpp"
#include "karma/ui/overlay.hpp"
#include "karma/ui/types.hpp"
#include "karma/common/config_helpers.hpp"

namespace karma::app {
EngineApp::EngineApp() {
    context_.ecsWorld = &ecsWorld_;
    context_.rendererContext.fov = karma::config::ReadRequiredFloatConfig("graphics.Camera.FovDegrees");
    context_.rendererContext.nearPlane = karma::config::ReadRequiredFloatConfig("graphics.Camera.NearPlane");
    context_.rendererContext.farPlane = karma::config::ReadRequiredFloatConfig("graphics.Camera.FarPlane");
}

EngineApp::~EngineApp() = default;

void EngineApp::setGame(GameInterface *game) {
    game_ = game;
}

EngineContext &EngineApp::context() {
    return context_;
}

const EngineContext &EngineApp::context() const {
    return context_;
}

int EngineApp::run() {
    if (!game_) {
        return 1;
    }
#ifndef KARMA_SERVER
    if (context_.graphics) {
        resources_ = std::make_unique<graphics::ResourceRegistry>(*context_.graphics);
        context_.resources = resources_.get();
        context_.defaultMaterial = context_.resources->getDefaultMaterial();
        rendererSystem_.setDefaultMaterial(context_.defaultMaterial);
    }
    if (context_.rendererCore) {
        context_.rendererContext = context_.rendererCore->context();
    }
#endif
    if (!game_->onInit(context_)) {
        return 1;
    }
    TimeUtils::time lastFrame = TimeUtils::GetCurrentTime();
    while (!game_->shouldQuit()) {
        const TimeUtils::time now = TimeUtils::GetCurrentTime();
        const float dt = TimeUtils::GetElapsedTime(lastFrame, now);
        lastFrame = now;
#ifndef KARMA_SERVER
        if (context_.rendererCore) {
            context_.rendererContext = context_.rendererCore->context();
        }
#endif
        game_->onUpdate(context_, dt);
 #ifndef KARMA_SERVER
        if (context_.rendererCore) {
            context_.rendererCore->context() = context_.rendererContext;
        }
 #endif
        systemGraph_.update(dt);
#ifndef KARMA_SERVER
        rendererSystem_.update(ecsWorld_, context_.graphics, dt);
#endif
        game_->onRender(context_);
    }
    game_->onShutdown(context_);
    return 0;
}
} // namespace karma::app
