#pragma once
#include <memory>
#include <vector>
#include "engine/types.hpp"
#include "engine/client_engine.hpp"
#include "player.hpp"
#include "world.hpp"
#include "shot.hpp"
#include "console.hpp"
#include "client.hpp"

enum FOCUS_STATE {
    FOCUS_STATE_GAME,
    FOCUS_STATE_CONSOLE
};

class Game {
private:
    FOCUS_STATE focusState;
    std::string playerName;

public:
    ClientEngine &engine;

    std::unique_ptr<Player> player;
    std::unique_ptr<World> world;
    std::unique_ptr<Console> console;

    std::vector<std::unique_ptr<Client>> clients;
    std::vector<std::unique_ptr<Shot>> shots;

    FOCUS_STATE getFocusState() const { return focusState; }

    Game(ClientEngine &engine, std::string playerName, std::string worldDir);
    ~Game();

    void earlyUpdate(TimeUtils::duration deltaTime);
    void lateUpdate(TimeUtils::duration deltaTime);

    void addShot(std::unique_ptr<Shot> shot) { shots.push_back(std::move(shot)); }

    Client *getClientById(client_id id);
};
