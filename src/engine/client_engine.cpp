#include "engine/client_engine.hpp"
#include "engine/types.hpp"
#include "spdlog/spdlog.h"
#include <chrono>

ClientEngine::ClientEngine(GLFWwindow *window) {
    this->window = window;

    userPointer = new GLFWUserPointer();
    glfwSetWindowUserPointer(window, userPointer);

    network = new ClientNetwork();
    spdlog::trace("ClientEngine: ClientNetwork initialized successfully");
    render = new Render(window);
    spdlog::trace("ClientEngine: Render initialized successfully");
    physics = new PhysicsWorld();
    spdlog::trace("ClientEngine: Physics initialized successfully");
    input = new Input(window);
    spdlog::trace("ClientEngine: Input initialized successfully");
    gui = new GUI(window);
    spdlog::trace("ClientEngine: GUI initialized successfully");
    gui->setSpawnHint(input->spawnHintText());
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
    delete gui;
    delete audio;
    delete particles;
}

void ClientEngine::earlyUpdate(TimeUtils::duration deltaTime) {
    input->update();
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

    const float deltaSeconds = std::chrono::duration<float>(deltaTime).count();
    particles->update(deltaSeconds);
    particles->render(render->getViewMatrix(), render->getProjectionMatrix(), render->getCameraPosition(), render->getCameraForward());

    gui->update();
    network->flushPeekedMessages();
}