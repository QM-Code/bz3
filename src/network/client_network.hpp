#pragma once
#include "core/types.hpp"
#include "network/transport.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <vector>

class ClientNetwork {
    friend class ClientEngine;

public:
    struct DisconnectEvent {
        std::string reason;
    };

    struct ServerEndpointInfo {
        std::string host;
        uint16_t port;
    };

private:
    std::unique_ptr<net::IClientTransport> transport;
    std::optional<DisconnectEvent> pendingDisconnect;
    std::optional<ServerEndpointInfo> serverEndpoint;

    struct MsgData {
        ServerMsg* msg;
        bool peeked = false;
    };

    std::vector<struct MsgData> receivedMessages;

    ClientNetwork();
    ~ClientNetwork();

    void flushPeekedMessages();
    void update();
    void sendImpl(const ClientMsg &input, bool flush);
    void logUnsupportedMessageType();

public:
    bool connect(const std::string &address, uint16_t port, int timeoutMs = 5000);
    void disconnect(const std::string &reason = "");
    std::optional<DisconnectEvent> consumeDisconnectEvent();
    bool isConnected() const { return transport && transport->isConnected(); }
    std::optional<ServerEndpointInfo> getServerEndpoint() const { return serverEndpoint; }

    template<typename T> T* peekMessage(std::function<bool(const T&)> predicate = [](const T&) { return true; }) {
        static_assert(std::is_base_of_v<ServerMsg, T>, "T must be a subclass of ServerMsg");

        for (auto &msgData : receivedMessages) {
            if (msgData.msg->type == T::Type) {
                auto* casted = static_cast<T*>(msgData.msg);

                if (predicate(*casted)) {
                    msgData.peeked = true;
                    return casted;
                }
            }
        }

        return nullptr;
    };

    template<typename T> std::vector<T> consumeMessages(std::function<bool(const T&)> predicate = [](const T&) { return true; }) {
        static_assert(std::is_base_of_v<ServerMsg, T>, "T must be a subclass of ServerMsg");

        std::vector<T> results;
        auto it = receivedMessages.begin();
        while (it != receivedMessages.end()) {
            if (it->msg && it->msg->type == T::Type) {
                auto* casted = static_cast<T*>(it->msg);
                if (predicate(*casted)) {
                    results.push_back(*casted);
                    delete it->msg;
                    it = receivedMessages.erase(it);
                    continue;
                }
            }
            ++it;
        }
        return results;
    }

    template<typename T> void send(const T &input, bool flush = false) {
        static_assert(std::is_base_of_v<ClientMsg, T>, "T must be a subclass of ClientMsg");
        sendImpl(input, flush);
    };
};
