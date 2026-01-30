#include "engine/app/engine_app.hpp"
#include "engine/core/types.hpp"
#include "engine/renderer/render_core.hpp"
#include "engine/ui/overlay.hpp"
#include "engine/ui/types.hpp"

namespace karma::app {
EngineApp::EngineApp() {
    context_.ecsWorld = &ecsWorld_;
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
        renderSystem_.setDefaultMaterial(context_.defaultMaterial);
    }
    if (context_.renderCore) {
        context_.renderContext = context_.renderCore->context();
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
        if (context_.renderCore) {
            context_.renderContext = context_.renderCore->context();
        }
#endif
        game_->onUpdate(context_, dt);
 #ifndef KARMA_SERVER
        if (context_.renderCore) {
            context_.renderCore->context() = context_.renderContext;
        }
 #endif
        systemGraph_.update(dt);
#ifndef KARMA_SERVER
        renderSystem_.update(ecsWorld_, context_.graphics, dt);
#endif
        game_->onRender(context_);
    }
    game_->onShutdown(context_);
    return 0;
}
} // namespace karma::app
