#include "engine/client_engine.hpp"
#include "engine/types.hpp"
#include "spdlog/spdlog.h"

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
    audio = new Audio();
    spdlog::trace("ClientEngine: Audio initialized successfully");
}

ClientEngine::~ClientEngine() {
    delete network;
    delete render;
    delete physics;
    delete input;
    delete gui;
    delete audio;
}

void ClientEngine::earlyUpdate(TimeUtils::duration deltaTime) {
    input->update();
    network->update();
}

void ClientEngine::step(TimeUtils::duration deltaTime) {
    physics->update(deltaTime);
}

void ClientEngine::lateUpdate(TimeUtils::duration deltaTime) {
    render->update();
    gui->update();
    network->flushPeekedMessages();
}