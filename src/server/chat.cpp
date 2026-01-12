#include "chat.hpp"
#include "game.hpp"
#include <string>
#include <sstream>
#include "spdlog/spdlog.h"
#include "engine/server_engine.hpp"
#include "plugin.hpp"

Chat::Chat(Game &game) : game(game) {

}

Chat::~Chat() {
    messages.clear();
}

void Chat::update() {
    // Listen for incoming chat
    for (const auto &chatMsg : game.engine.network->consumeMessages<ClientMsg_Chat>()) {
        auto mutableMsg = chatMsg;
        // Info out the chat message
        spdlog::info("Client: {}, Message: {}", game.getClient(mutableMsg.clientId)->getName(), mutableMsg.text);
        
        std::string message = std::string(mutableMsg.text);
        messages.push_back(message);

        bool handled = g_runPluginCallbacks<ClientMsg_Chat>(mutableMsg);

        if (!handled) {
            ServerMsg_Chat serverChatMsg;
            serverChatMsg.fromId = mutableMsg.clientId;
            serverChatMsg.toId = mutableMsg.toId;
            serverChatMsg.text = mutableMsg.text;

            if (mutableMsg.toId == BROADCAST_CLIENT_ID) {
                game.engine.network->sendExcept<ServerMsg_Chat>(mutableMsg.clientId, &serverChatMsg);
            } else {
                game.engine.network->send<ServerMsg_Chat>(mutableMsg.toId, &serverChatMsg);
            }
        }
    }
}
