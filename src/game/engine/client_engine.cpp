#include "engine/client_engine.hpp"
#include "core/types.hpp"
#include "game/net/messages.hpp"
#include "game/input/bindings.hpp"
#include "game/input/state.hpp"
#include "spdlog/spdlog.h"
#include "ui/render_bridge.hpp"
#include <glad/glad.h>
#include <cstdint>
#include <vector>
#include <utility>

namespace {

class RenderBridgeImpl final : public ui::RenderBridge {
public:
    explicit RenderBridgeImpl(Render *renderIn) : render(renderIn) {}

    unsigned int getRadarTextureId() const override {
        return render ? render->getRadarTextureId() : 0u;
    }

private:
    Render *render = nullptr;
};

bool ShouldUseOpenGL() {
#if defined(BZ3_RENDER_BACKEND_BGFX)
    if (const char* noGl = std::getenv("BZ3_BGFX_NO_GL"); noGl && noGl[0] != '\0') {
        return false;
    }
#endif
    return true;
}

} // namespace

ClientEngine::ClientEngine(platform::Window &window) {
    this->window = &window;

    network = new ClientNetwork();
    spdlog::trace("ClientEngine: ClientNetwork initialized successfully");
    spdlog::trace("ClientEngine: Render initializing");
    render = new Render(window);
    spdlog::trace("ClientEngine: Render initialized successfully");
    uiRenderBridge = std::make_unique<RenderBridgeImpl>(render);
    physics = new PhysicsWorld();
    spdlog::trace("ClientEngine: Physics initialized successfully");
    input = new Input(window, game_input::DefaultKeybindings());
    spdlog::trace("ClientEngine: Input initialized successfully");
    ui = new UiSystem(window);
    ui->setRenderBridge(uiRenderBridge.get());
    spdlog::trace("ClientEngine: UiSystem initialized successfully");
    ui->setSpawnHint(game_input::SpawnHintText(*input));
    audio = new Audio();
    spdlog::trace("ClientEngine: Audio initialized successfully");
}

ClientEngine::~ClientEngine() {
    delete network;
    delete ui;
    delete render;
    delete physics;
    delete input;
    delete audio;
}

void ClientEngine::earlyUpdate(TimeUtils::duration deltaTime) {
    if (window) {
        window->pollEvents();
    }
    const std::vector<platform::Event> emptyEvents;
    const auto &events = window ? window->events() : emptyEvents;
    if (ui) {
        ui->handleEvents(events);
    }
    if (input) {
        input->update(events);
    }
    inputState = game_input::BuildInputState(*input);
    if (window) {
        window->clearEvents();
    }
    network->update();
}

void ClientEngine::step(TimeUtils::duration deltaTime) {
    physics->update(deltaTime);
}

void ClientEngine::lateUpdate(TimeUtils::duration deltaTime) {
    if (ShouldUseOpenGL()) {
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
    }

    render->update();
    render->present();

    ui->update();

    render->setBrightness(ui->getRenderBrightness());

    if (ui->consumeKeybindingsReloadRequest()) {
        input->reloadKeyBindings();
        ui->setSpawnHint(game_input::SpawnHintText(*input));
    }
    network->flushPeekedMessages();
}
