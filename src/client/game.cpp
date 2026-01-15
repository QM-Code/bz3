#include "game.hpp"
#include "spdlog/spdlog.h"
#include <algorithm>
#include "engine/components/gui.hpp"

Game::Game(ClientEngine &engine, std::string playerName, std::string worldDir) : playerName(playerName), engine(engine) {
    world = std::make_unique<World>(*this, worldDir);
    spdlog::trace("Game: World created successfully");
    console = std::make_unique<Console>(*this);
    spdlog::trace("Game: Console created successfully");

    focusState = FOCUS_STATE_GAME;
};

Game::~Game() {
    player.reset();
    spdlog::trace("Game: Player destroyed successfully");
    world.reset();
    spdlog::trace("Game: World destroyed successfully");
    console.reset();
    spdlog::trace("Game: Console destroyed successfully");
    clients.clear();
    shots.clear();
}

void Game::earlyUpdate(TimeUtils::duration deltaTime) {
    world->update();

    if (!world->isInitialized()) {
        return;
    }

    if (!player) {
        spdlog::trace("Game: Creating player with name '{}'", playerName);
        player = std::make_unique<Player>(*this, world->playerId, world->getDefaultPlayerParameters(), playerName);
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
        clients.push_back(std::make_unique<Client>(*this, msg.clientId, msg.state));
        spdlog::trace("Game: New client connected with ID {}", msg.clientId);
    }

    for (const auto &msg : engine.network->consumeMessages<ServerMsg_PlayerLeave>()) {
        auto it = std::find_if(clients.begin(), clients.end(),
            [&msg](const std::unique_ptr<Client> &client) {
                return client->isEqual(msg.clientId);
            }
        );

        if (it != clients.end()) {
            clients.erase(it);
            spdlog::trace("Game: Client disconnected with ID {}", msg.clientId);
        }
    }

    for (const auto &msg : engine.network->consumeMessages<ServerMsg_PlayerParameters>()) {
        if (player && msg.clientId == world->playerId) {
            player->handleParameters(msg);
        }
    }

    for (const auto &msg : engine.network->consumeMessages<ServerMsg_PlayerState>()) {
        if (auto *client = getClientById(msg.clientId)) {
            client->applyState(msg.state);
        }
    }

    for (const auto &msg : engine.network->consumeMessages<ServerMsg_PlayerLocation>()) {
        if (auto *client = getClientById(msg.clientId)) {
            client->applyLocation(msg);
        }
    }

    for (const auto &msg : engine.network->consumeMessages<ServerMsg_PlayerDeath>()) {
        if (player && msg.clientId == world->playerId) {
            player->handleDeath();
            continue;
        }
        if (auto *client = getClientById(msg.clientId)) {
            client->handleDeath();
        }
    }

    for (const auto &msg : engine.network->consumeMessages<ServerMsg_SetScore>()) {
        if (player && msg.clientId == world->playerId) {
            player->setScore(msg.score);
            continue;
        }
        if (auto *client = getClientById(msg.clientId)) {
            client->applySetScore(msg.score);
        }
    }

    for (const auto &msg : engine.network->consumeMessages<ServerMsg_PlayerSpawn>()) {
        if (player && msg.clientId == world->playerId) {
            player->handleSpawn(msg);
            continue;
        }
        if (auto *client = getClientById(msg.clientId)) {
            client->handleSpawn(msg);
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

    player->earlyUpdate();
}

void Game::lateUpdate(TimeUtils::duration deltaTime) {
    if (!world->isInitialized()) {
        return;
    }

    player->lateUpdate();

    for (const auto &client : clients) {
        client->update();
    }

    for (const auto &shot : shots) {
        shot->update(deltaTime);
    }

    std::vector<ScoreboardEntry> scoreboard;
    scoreboard.reserve(clients.size() + 1);
    for (const auto &client : clients) {
        scoreboard.push_back(ScoreboardEntry{client->getName(), client->getScore()});
    }
    scoreboard.push_back(ScoreboardEntry{player->getName(), player->getScore()});
    engine.gui->setScoreboardEntries(scoreboard);
}

Client *Game::getClientById(client_id id) {
    for (const auto &client : clients) {
        if (client->isEqual(id)) {
            return client.get();
        }
    }
    return nullptr;
}
