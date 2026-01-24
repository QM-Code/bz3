#include "engine/client_engine.hpp"
#include "core/types.hpp"
#include "spdlog/spdlog.h"
#include <glad/glad.h>
#include <chrono>
#include <vector>

ClientEngine::ClientEngine(platform::Window &window) {
    this->window = &window;

    network = new ClientNetwork();
    spdlog::trace("ClientEngine: ClientNetwork initialized successfully");
    spdlog::trace("ClientEngine: Render initializing");
    render = new Render(window);
    spdlog::trace("ClientEngine: Render initialized successfully");
    physics = new PhysicsWorld();
    spdlog::trace("ClientEngine: Physics initialized successfully");
    input = new Input(window);
    spdlog::trace("ClientEngine: Input initialized successfully");
    ui = new UiSystem(window);
    spdlog::trace("ClientEngine: UiSystem initialized successfully");
    ui->setSpawnHint(input->spawnHintText());
    audio = new Audio();
    spdlog::trace("ClientEngine: Audio initialized successfully");
    particles = new ParticleEngine();
    spdlog::trace("ClientEngine: ParticleEngine initialized successfully");
}

ClientEngine::~ClientEngine() {
    delete network;
    delete render;
    delete physics;
    delete input;
    delete ui;
    delete audio;
    delete particles;
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
    if (window) {
        window->clearEvents();
    }
    network->update();
}

void ClientEngine::step(TimeUtils::duration deltaTime) {
    physics->update(deltaTime);
}

void ClientEngine::lateUpdate(TimeUtils::duration deltaTime) {
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);

    render->update();

    ui->setRadarTextureId(render->getRadarTextureId());

    const float deltaSeconds = std::chrono::duration<float>(deltaTime).count();
    particles->update(deltaSeconds);
    particles->render(render->getViewMatrix(), render->getProjectionMatrix(), render->getCameraPosition(), render->getCameraForward());

    ui->update();
    network->flushPeekedMessages();
}
