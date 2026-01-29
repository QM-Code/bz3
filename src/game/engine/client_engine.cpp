#include "engine/client_engine.hpp"
#include "core/types.hpp"
#include "game/net/messages.hpp"
#include "game/input/bindings.hpp"
#include "game/input/state.hpp"
#include "spdlog/spdlog.h"
#include "ui/bridges/render_bridge.hpp"
#include "common/config_store.hpp"
#include <cstdlib>
#include "common/i18n.hpp"
#if defined(BZ3_UI_BACKEND_IMGUI)
#include "ui/bridges/imgui_render_bridge.hpp"
#endif
#include <cstdint>
#include <vector>
#include <utility>

namespace {

class RenderBridgeImpl final : public ui::RenderBridge
#if defined(BZ3_UI_BACKEND_IMGUI)
    , public ui::ImGuiRenderBridge
#endif
{
public:
    explicit RenderBridgeImpl(Render *renderIn) : render(renderIn) {}

    graphics::TextureHandle getRadarTexture() const override {
        return render ? render->getRadarTexture() : graphics::TextureHandle{};
    }

#if defined(BZ3_UI_BACKEND_IMGUI)
    graphics_backend::UiRenderTargetBridge* getUiRenderTargetBridge() const override {
        return render ? render->getUiRenderTargetBridge() : nullptr;
    }
#endif

private:
    Render *render = nullptr;
};

} // namespace

ClientEngine::ClientEngine(platform::Window &window) {
    this->window = &window;

    network = new ClientNetwork();
    spdlog::trace("ClientEngine: ClientNetwork initialized successfully");
    spdlog::trace("ClientEngine: Render initializing");
    render = new Render(window);
    spdlog::trace("ClientEngine: Render initialized successfully");
    auto* renderBridgeImpl = new RenderBridgeImpl(render);
    uiRenderBridge.reset(renderBridgeImpl);
    physics = new PhysicsWorld();
    spdlog::trace("ClientEngine: Physics initialized successfully");
    input = new Input(window, game_input::DefaultKeybindings());
    spdlog::trace("ClientEngine: Input initialized successfully");
    ui = new UiSystem(window);
    ui->setRenderBridge(renderBridgeImpl);
#if defined(BZ3_UI_BACKEND_IMGUI)
    ui->setImGuiRenderBridge(renderBridgeImpl);
#endif
    spdlog::trace("ClientEngine: UiSystem initialized successfully");
    lastLanguage = bz::i18n::Get().language();
    ui->setDialogText(game_input::SpawnHintText(*input));
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
    render->update();
    ui->update();
    const std::string currentLanguage = bz::i18n::Get().language();
    if (currentLanguage != lastLanguage) {
        lastLanguage = currentLanguage;
        if (input) {
            ui->setDialogText(game_input::SpawnHintText(*input));
        }
    }
    render->setUiOverlayTexture(ui->getRenderOutput());
    if (!std::getenv("BZ3_DISABLE_UI_OVERLAY")) {
        render->renderUiOverlay();
    }
    render->present();

    render->setBrightness(ui->getRenderBrightness());

    if (ui->consumeKeybindingsReloadRequest()) {
        input->reloadKeyBindings();
        ui->setDialogText(game_input::SpawnHintText(*input));
    }
    network->flushPeekedMessages();
    bz::config::ConfigStore::Tick();
}
