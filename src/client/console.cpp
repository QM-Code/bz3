#include "console.hpp"
#include "game.hpp"
#include "spdlog/spdlog.h"

Console::Console(Game &game) : game(game) {

}

void Console::focusChatInput() {
    game.engine.gui->focusChatInput();
    chatInFocus = true;
}

void Console::update() {
    if (chatInFocus) {
        if (game.engine.gui->getChatInputBuffer().length() > 0) {
            spdlog::trace("Console::update: Processing submitted chat input");
            std::string message = game.engine.gui->getChatInputBuffer();
            std::string consoleMessage = message;
            bool includePlayerName = true;

            const std::string prefix = "/msg ";
            if (message.rfind(prefix, 0) == 0) {
                const std::size_t afterPrefix = prefix.size();
                const std::size_t nameEnd = message.find(' ', afterPrefix);
                if (nameEnd != std::string::npos && nameEnd + 1 < message.size()) {
                    const std::string targetName = message.substr(afterPrefix, nameEnd - afterPrefix);
                    const std::string msgBody = message.substr(nameEnd + 1);
                    consoleMessage = "[-> " + targetName + "] " + msgBody;
                    includePlayerName = false;
                }
            }

            game.engine.gui->addConsoleLine((includePlayerName && game.player) ? game.player->getName() : std::string(), consoleMessage);

            ClientMsg_Chat chatMsg;
            chatMsg.toId = BROADCAST_CLIENT_ID;
            chatMsg.text = message;
            game.engine.network->send<ClientMsg_Chat>(chatMsg);
            game.engine.gui->clearChatInputBuffer();
        }

        if (!game.engine.gui->getChatInputFocus()) {
            chatInFocus = false;
        }
    }

    for (const auto &msg : game.engine.network->consumeMessages<ServerMsg_Chat>()) {
        std::string name;
        if (auto *actor = game.getActorById(msg.fromId)) {
            name = (game.player && actor == game.player) ? "YOU" : actor->getState().name;
        } else if (msg.fromId == SERVER_CLIENT_ID) {
            name = "SERVER";
        } else {
            name = "UNKNOWN";
        }

        if (game.player && msg.toId == game.player->getClientId()) {
            name = "[" + name + " ->]";
        }

        game.engine.gui->addConsoleLine(name, msg.text);
    }
}
