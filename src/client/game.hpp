#pragma once
#include <memory>
#include <vector>
#include "core/types.hpp"
#include "engine/client_engine.hpp"
#include "world_session.hpp"
#include "shot.hpp"
#include "console.hpp"

#include "actor.hpp"
#include "player.hpp"
#include "client.hpp"

enum FOCUS_STATE {
    FOCUS_STATE_GAME,
    FOCUS_STATE_CONSOLE
};

class Game {
private:
    FOCUS_STATE focusState;
    std::string playerName;
    bool registeredUser = false;
    bool communityAdmin = false;
    bool localAdmin = false;

    std::vector<std::unique_ptr<Actor>> actors;

public:
    ClientEngine &engine;

    Player *player = nullptr;
    std::unique_ptr<ClientWorldSession> world;
    std::unique_ptr<Console> console;
    std::vector<std::unique_ptr<Shot>> shots;

    FOCUS_STATE getFocusState() const { return focusState; }

    Game(ClientEngine &engine,
         std::string playerName,
         std::string worldDir,
         bool registeredUser,
         bool communityAdmin,
         bool localAdmin);
    ~Game();

    void earlyUpdate(TimeUtils::duration deltaTime);
    void lateUpdate(TimeUtils::duration deltaTime);

    void addShot(std::unique_ptr<Shot> shot) { shots.push_back(std::move(shot)); }

    const std::vector<std::unique_ptr<Actor>> &getActors() const { return actors; }
    Actor *getActorById(client_id id);
};
