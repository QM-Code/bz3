#include "game.hpp"
#include "spdlog/spdlog.h"
#include <algorithm>
#include "engine/components/gui.hpp"

Game::Game(ClientEngine &engine,
           std::string playerName,
           std::string worldDir,
           bool registeredUser,
           bool communityAdmin,
           bool localAdmin)
    : playerName(std::move(playerName)),
      registeredUser(registeredUser),
      communityAdmin(communityAdmin),
      localAdmin(localAdmin),
      engine(engine) {
    world = std::make_unique<World>(*this, worldDir);
    spdlog::trace("Game: World created successfully");
    console = std::make_unique<Console>(*this);
    spdlog::trace("Game: Console created successfully");

    focusState = FOCUS_STATE_GAME;
};

Game::~Game() {
    world.reset();
    spdlog::trace("Game: World destroyed successfully");
    console.reset();
    spdlog::trace("Game: Console destroyed successfully");
    actors.clear();
    shots.clear();
}

void Game::earlyUpdate(TimeUtils::duration deltaTime) {
    world->update();

    if (!world->isInitialized()) {
        return;
    }

    if (!player) {
        spdlog::trace("Game: Creating player with name '{}'", playerName);
        auto playerActor = std::make_unique<Player>(
            *this,
            world->playerId,
            world->getDefaultPlayerParameters(),
            playerName,
            registeredUser,
            communityAdmin,
            localAdmin);
        player = playerActor.get();
        actors.push_back(std::move(playerActor));
        spdlog::trace("Game: Player created successfully");
    }

    if (focusState == FOCUS_STATE_GAME && engine.input->getInputState().chat) {
        focusState = FOCUS_STATE_CONSOLE;
        spdlog::trace("Game: Switching focus to console");
        console->focusChatInput();
    }

    console->update();

    if (focusState == FOCUS_STATE_CONSOLE && !console->isChatInFocus()) {
        focusState = FOCUS_STATE_GAME;
        spdlog::trace("Game: Returning focus to game");
    }

    for (const auto &msg : engine.network->consumeMessages<ServerMsg_PlayerJoin>()) {
        if (getActorById(msg.clientId)) {
            continue;
        }
        actors.push_back(std::make_unique<Client>(*this, msg.clientId, msg.state));
        spdlog::trace("Game: New client connected with ID {}", msg.clientId);
    }

    for (const auto &msg : engine.network->consumeMessages<ServerMsg_PlayerLeave>()) {
        auto it = std::remove_if(actors.begin(), actors.end(),
            [&msg](const std::unique_ptr<Actor> &actor) {
                return actor->isEqual(msg.clientId);
            });

        if (it != actors.end()) {
            actors.erase(it, actors.end());
            spdlog::trace("Game: Client disconnected with ID {}", msg.clientId);
        }
    }

    for (const auto &msg : engine.network->consumeMessages<ServerMsg_PlayerParameters>()) {
        if (auto *actor = getActorById(msg.clientId)) {
            actor->setParameters(msg.params);
        }
    }

    for (const auto &msg : engine.network->consumeMessages<ServerMsg_PlayerState>()) {
        if (auto *actor = getActorById(msg.clientId)) {
            actor->setState(msg.state);
        }
    }

    for (const auto &msg : engine.network->consumeMessages<ServerMsg_PlayerLocation>()) {
        if (auto *actor = getActorById(msg.clientId)) {
            actor->setLocation(msg.position, msg.rotation, msg.velocity);
        }
    }

    for (const auto &msg : engine.network->consumeMessages<ServerMsg_PlayerDeath>()) {
        if (auto *actor = getActorById(msg.clientId)) {
            actor->die();
        }
    }

    for (const auto &msg : engine.network->consumeMessages<ServerMsg_SetScore>()) {
        if (auto *actor = getActorById(msg.clientId)) {
            actor->setScore(msg.score);
        }
    }

    for (const auto &msg : engine.network->consumeMessages<ServerMsg_PlayerSpawn>()) {
        if (auto *actor = getActorById(msg.clientId)) {
            actor->spawn(msg.position, msg.rotation, msg.velocity);
        }
    }

    for (const auto &msg : engine.network->consumeMessages<ServerMsg_CreateShot>()) {
        shots.push_back(std::make_unique<Shot>(
            *this,
            msg.globalShotId,
            msg.position,
            msg.velocity
        ));
    }

    for (const auto &msg : engine.network->consumeMessages<ServerMsg_RemoveShot>()) {
        auto it = std::remove_if(shots.begin(), shots.end(),
            [&msg](const std::unique_ptr<Shot> &shot) {
                return shot->isEqual(msg.shotId, msg.isGlobalId);
            });
        if (it != shots.end()) {
            shots.erase(it, shots.end());
        }
    }

    (void)deltaTime;
}

void Game::lateUpdate(TimeUtils::duration deltaTime) {
    if (!world->isInitialized()) {
        return;
    }

    for (const auto &actor : actors) {
        actor->update(deltaTime);
    }

    for (const auto &shot : shots) {
        shot->update(deltaTime);
    }

    std::vector<ScoreboardEntry> scoreboard;
    scoreboard.reserve(actors.size());
    for (const auto &actor : actors) {
        const auto &s = actor->getState();
        scoreboard.push_back(ScoreboardEntry{
            s.name,
            s.score,
            s.registeredUser,
            s.communityAdmin,
            s.localAdmin
        });
    }
    engine.gui->setScoreboardEntries(scoreboard);
}

Actor *Game::getActorById(client_id id) {
    for (auto &actor : actors) {
        if (actor->isEqual(id)) {
            return actor.get();
        }
    }
    return nullptr;
}
