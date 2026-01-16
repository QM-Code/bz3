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

void Chat::handleMessage(const ClientMsg_Chat &chatMsg) {
    auto mutableMsg = chatMsg;

    const Client *fromClient = game.getClient(mutableMsg.clientId);
    if (!fromClient) {
        spdlog::warn("Chat::handleMessage: Received chat from unknown client id {}", mutableMsg.clientId);
        return;
    }

    spdlog::info("Client: {}, Message: {}", fromClient->getName(), mutableMsg.text);

    messages.push_back(std::string(mutableMsg.text));

    bool handled = g_runPluginCallbacks<ClientMsg_Chat>(mutableMsg);
    if (handled) {
        return;
    }

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
