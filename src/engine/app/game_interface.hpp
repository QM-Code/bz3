#pragma once

namespace karma::app {
struct EngineContext;

class GameInterface {
public:
    virtual ~GameInterface() = default;

    virtual bool onInit(EngineContext &context) = 0;
    virtual void onShutdown(EngineContext &context) = 0;
    virtual void onUpdate(EngineContext &context, float dt) = 0;
    virtual void onRender(EngineContext &context) = 0;
    virtual bool shouldQuit() const { return false; }
};
} // namespace karma::app
