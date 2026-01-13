#include "player.hpp"
#include "engine/client_engine.hpp"
#include "engine/types.hpp"
#include "game.hpp"
#include <string>
#include <utility>
#include <memory>
#include "spdlog/spdlog.h"
#include "shot.hpp"

Player::Player(Game &game,
               client_id id,
               PlayerParameters params,
               const std::string name)
    : game(game),
      clientId(id),
      grounded(false),
            physics(&game.engine.physics->createPlayer()),
      audioEngine(*game.engine.audio),
    jumpAudio(audioEngine.loadClip(game.world->getAssetPath("audio.player.Jump"), 5)),
    dieAudio(audioEngine.loadClip(game.world->getAssetPath("audio.player.Die"), 1)),
    spawnAudio(audioEngine.loadClip(game.world->getAssetPath("audio.player.Spawn"), 1)),
    landAudio(audioEngine.loadClip(game.world->getAssetPath("audio.player.Land"), 1)),
      lastJumpTime(TimeUtils::GetCurrentTime()),
      jumpCooldown(TimeUtils::getDuration(0.1f)) {
    state.params = std::move(params);
    state.name = name;
    state.alive = false;
    lastPosition = glm::vec3(0.0f);
    lastRotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);

    ClientMsg_Init initMsg;
    initMsg.name = name;
    initMsg.protocolVersion = NET_PROTOCOL_VERSION;
    game.engine.network->send<ClientMsg_Init>(initMsg);

    // Initialize controller extents from parameters once params are set
    setExtents(glm::vec3(
        getParameter("x_extent"),
        getParameter("y_extent"),
        getParameter("z_extent")));
}

Player::~Player() = default;

float Player::getParameter(const std::string &paramName) const {
    auto it = state.params.find(paramName);
    if (it != state.params.end()) {
        return it->second;
    } else {
        spdlog::warn("Player::getParameter: Parameter '{}' not found, returning 0.0", paramName);
        return 0.0f;
    }
}

glm::vec3 Player::getForwardVector() const {
    return physics->getForwardVector();
}

void Player::setExtents(const glm::vec3& extents) {
    if (physics) {
        physics->setHalfExtents(extents * 0.5f);
    }
}

void Player::earlyUpdate() {
    if (state.alive) {
        game.engine.gui->displayDeathScreen(false);

        bool wasGrounded = grounded;
        grounded = physics->isGrounded();
        
        if (grounded) {
            glm::vec2 movement(0.0f);
            if (game.getFocusState() == FOCUS_STATE_GAME)
                movement = game.engine.input->getInputState().movement;
            glm::vec3 movementVector = physics->getForwardVector();
            movementVector *= movement.y * getParameter("speed");
            movementVector.y = physics->getVelocity().y;

            physics->setVelocity(movementVector);

            physics->setAngularVelocity(glm::vec3(
                0.0f,
                -movement.x * getParameter("turnSpeed"),
                0.0f
            ));

            if (game.getFocusState() == FOCUS_STATE_GAME) {
                if (grounded && game.engine.input->getInputState().jump && TimeUtils::GetElapsedTime(lastJumpTime, TimeUtils::GetCurrentTime()) >= jumpCooldown) {
                    glm::vec3 velocity = physics->getVelocity();
                    velocity.y = getParameter("jumpSpeed");
                    physics->setVelocity(velocity);
                    lastJumpTime = TimeUtils::GetCurrentTime();
                    grounded = false;
                    jumpAudio.play(state.position);
                }
            }

            if (wasGrounded == false) {
                landAudio.play(state.position);
            }
        }

        if (game.getFocusState() == FOCUS_STATE_GAME) {
            if (game.engine.input->getInputState().fire) {
                glm::vec3 shotPosition = state.position + getForwardVector() * 2.0f;
                glm::vec3 shotVelocity = getForwardVector() * getParameter("shotSpeed") + getVelocity();

                auto shot = std::make_unique<Shot>(game, shotPosition, shotVelocity);
                game.addShot(std::move(shot));
            }
        }

    } else {
        game.engine.gui->displayDeathScreen(true);

        if (game.engine.input->getInputState().spawn) {
            ClientMsg_RequestPlayerSpawn spawnMsg;
            game.engine.network->send<ClientMsg_RequestPlayerSpawn>(spawnMsg);
        }
    }
}

void Player::lateUpdate() {
    state.position = physics->getPosition();
    state.rotation = physics->getRotation();
    state.velocity = physics->getVelocity();
    game.engine.render->setCameraPosition(state.position);
    game.engine.render->setCameraRotation(state.rotation);

    if (glm::distance(lastPosition, state.position) > POSITION_UPDATE_THRESHOLD ||
        angleBetween(lastRotation, state.rotation) > ROTATION_UPDATE_THRESHOLD) {
        ClientMsg_PlayerLocation locMsg;
        locMsg.position = state.position;
        locMsg.rotation = state.rotation;
        game.engine.network->send<ClientMsg_PlayerLocation>(locMsg);
        lastPosition = state.position;
        lastRotation = state.rotation;
    }

    audioEngine.setListenerPosition(state.position);
    audioEngine.setListenerRotation(state.rotation);
}

void Player::handleParameters(const ServerMsg_PlayerParameters &msg) {
    for (const auto &pair : msg.params) {
        state.params[pair.first] = pair.second;
    }
    game.engine.physics->setGravity(getParameter("gravity"));
}

void Player::handleDeath() {
    if (!state.alive) {
        return;
    }
    dieAudio.play(state.position);
    state.alive = false;
}

void Player::handleSpawn(const ServerMsg_PlayerSpawn &msg) {
    spawnAudio.play(msg.position);
    state.alive = true;
    physics->setPosition(msg.position);
    physics->setRotation(msg.rotation);
    physics->setVelocity(glm::vec3(0.0f));
    physics->setAngularVelocity(glm::vec3(0.0f));
}
