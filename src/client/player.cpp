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
               const std::string name,
               bool registeredUser,
               bool communityAdmin,
               bool localAdmin)
        : Actor(game, id),
            grounded(false),
            physics(&game.engine.physics->createPlayer()),
      audioEngine(*game.engine.audio),
    jumpAudio(audioEngine.loadClip(game.world->getAssetPath("audio.player.Jump"), 5)),
    dieAudio(audioEngine.loadClip(game.world->getAssetPath("audio.player.Die"), 1)),
    spawnAudio(audioEngine.loadClip(game.world->getAssetPath("audio.player.Spawn"), 1)),
    landAudio(audioEngine.loadClip(game.world->getAssetPath("audio.player.Land"), 1)),
      lastJumpTime(TimeUtils::GetCurrentTime()),
      jumpCooldown(TimeUtils::getDuration(0.1f)) {
        setParameters(std::move(params));
    state.name = name;
    state.registeredUser = registeredUser;
    state.communityAdmin = communityAdmin;
    state.localAdmin = localAdmin;
    state.alive = false;
    state.score = 0;
    lastPosition = glm::vec3(0.0f);
    lastRotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);

    renderId = game.engine.render->create();
    game.engine.render->setRadarCircleGraphic(renderId, 1.2f);


    // Initialize controller extents from parameters once params are set
    setExtents(glm::vec3(
        getParameter("x_extent"),
        getParameter("y_extent"),
        getParameter("z_extent")));
}

Player::~Player() {
    game.engine.render->destroy(renderId);
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
    bool wasGrounded = grounded;
    grounded = physics->isGrounded();

    game.engine.render->setPosition(renderId, state.position);

    if (state.alive) {
        game.engine.gui->displayDeathScreen(false);
        
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
                const glm::vec3 cameraPos = state.position + glm::vec3(0.0f, muzzleOffset.y, 0.0f);
                const glm::vec3 muzzlePos = state.position + getForwardVector() * muzzleOffset.z +
                                            glm::vec3(0.0f, muzzleOffset.y, 0.0f);

                glm::vec3 shotPosition = muzzlePos;
                if (game.engine.physics) {
                    glm::vec3 hitPoint{};
                    glm::vec3 hitNormal{};
                    if (game.engine.physics->raycast(cameraPos, muzzlePos, hitPoint, hitNormal)) {
                        const glm::vec3 dirVec = muzzlePos - cameraPos;
                        const float dirLenSq = glm::dot(dirVec, dirVec);
                        if (dirLenSq > 1e-6f) {
                            const glm::vec3 dir = dirVec * (1.0f / std::sqrt(dirLenSq));
                            constexpr float backOff = 0.05f;
                            shotPosition = hitPoint - dir * backOff;
                        } else {
                            shotPosition = hitPoint;
                        }
                    }
                }

                glm::vec3 shotVelocity = getForwardVector() * getParameter("shotSpeed") + getVelocity();

                auto shot = std::make_unique<Shot>(game, shotPosition, shotVelocity);
                game.addShot(std::move(shot));
            }
        }

    } else {
        if (grounded) {
            physics->setVelocity(glm::vec3(0.0f));
            physics->setAngularVelocity(glm::vec3(0.0f));
        }

        game.engine.gui->displayDeathScreen(true);

        if (game.engine.input->getInputState().spawn) {
            ClientMsg_RequestPlayerSpawn spawnMsg;
            game.engine.network->send<ClientMsg_RequestPlayerSpawn>(spawnMsg);
        }
    }
}

    void Player::lateUpdate() {
    setLocation(physics->getPosition(), physics->getRotation(), physics->getVelocity());
    game.engine.render->setCameraPosition(state.position + glm::vec3(0.0f, muzzleOffset.y, 0.0f));
    game.engine.render->setCameraRotation(state.rotation);
    game.engine.render->setRadarFOVLinesAngle(CAMERA_FOV);

    if (state.alive) {
        if (glm::distance(lastPosition, state.position) > POSITION_UPDATE_THRESHOLD ||
            angleBetween(lastRotation, state.rotation) > ROTATION_UPDATE_THRESHOLD) {
            ClientMsg_PlayerLocation locMsg;
            locMsg.position = state.position;
            locMsg.rotation = state.rotation;
            game.engine.network->send<ClientMsg_PlayerLocation>(locMsg);
            lastPosition = state.position;
            lastRotation = state.rotation;
        }
    }

    audioEngine.setListenerPosition(state.position);
    audioEngine.setListenerRotation(state.rotation);
}

void Player::update(TimeUtils::duration /*deltaTime*/) {
    earlyUpdate();
    lateUpdate();
}

void Player::setState(const PlayerState &newState) {
    state = newState;
    if (physics) {
        physics->setPosition(state.position);
        physics->setRotation(state.rotation);
        physics->setVelocity(state.velocity);
    }
}

void Player::die() {
    if (!state.alive) {
        return;
    }
    Actor::die();
    dieAudio.play(state.position);
    state.alive = false;
    auto vel = physics->getVelocity();
    physics->setVelocity(glm::vec3(vel.x, getParameter("jumpSpeed"), vel.z));
}

void Player::spawn(glm::vec3 position, glm::quat rotation, glm::vec3 velocity) {
    spawnAudio.play(position);
    state.alive = true;
    setLocation(position, rotation, velocity);

    physics->setPosition(position);
    physics->setRotation(rotation);
    physics->setVelocity(velocity);
    physics->setAngularVelocity(glm::vec3(0.0f));
}
