#pragma once
#include "karma/core/types.hpp"
#include "game/net/messages.hpp"
#include "game/net/client_network.hpp"
#include "renderer/renderer.hpp"
#include "karma/physics/physics_world.hpp"
#include "karma/input/input.hpp"
#include "game/input/state.hpp"
#include "ui/core/system.hpp"
#include "client/roaming_camera.hpp"
#include "karma/ecs/world.hpp"
#include "karma/audio/audio.hpp"
#include "karma/platform/window.hpp"
#include <string>
#include <memory>
#include <vector>

namespace ui {
class RendererBridge;
}

class ClientEngine {
private:
    platform::Window *window;
    std::unique_ptr<ui::RendererBridge> uiRenderBridge;
    std::string lastLanguage;
    float lastBrightness = 1.0f;
    bool roamingMode = false;
    bool roamingModeInitialized = false;
    std::vector<platform::Event> lastEvents;
    game_client::RoamingCameraController roamingCamera;

public:
    ClientNetwork *network;
    Renderer *render;
    PhysicsWorld *physics;
    Input *input;
    game_input::InputState inputState;
    UiSystem *ui;
    Audio *audio;
    ecs::World *ecsWorld = nullptr;

    void setRoamingModeSession(bool enabled);
    bool isRoamingModeSession() const { return roamingMode; }

    ClientEngine(platform::Window &window);
    ~ClientEngine();

    void earlyUpdate(TimeUtils::duration deltaTime);
    void step(TimeUtils::duration deltaTime);
    void lateUpdate(TimeUtils::duration deltaTime);
    void updateRoamingCamera(TimeUtils::duration deltaTime, bool allowInput);

    const game_input::InputState& getInputState() const { return inputState; }
};
