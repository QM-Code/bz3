#pragma once
#include "game/net/messages.hpp"
#include "game/net/backend.hpp"

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
    using DisconnectEvent = network_backend::DisconnectEvent;
    using ServerEndpointInfo = network_backend::ServerEndpointInfo;

private:
    std::unique_ptr<network_backend::ClientBackend> backend_;

    ClientNetwork();
    ~ClientNetwork();

    void flushPeekedMessages();
    void update();
    void sendImpl(const ClientMsg &input, bool flush);

public:
    bool connect(const std::string &address, uint16_t port, int timeoutMs = 5000);
    void disconnect(const std::string &reason = "");
    std::optional<DisconnectEvent> consumeDisconnectEvent();
    bool isConnected() const;
    std::optional<ServerEndpointInfo> getServerEndpoint() const;

    template<typename T> T* peekMessage(std::function<bool(const T&)> predicate = [](const T&) { return true; }) {
        static_assert(std::is_base_of_v<ServerMsg, T>, "T must be a subclass of ServerMsg");

        if (!backend_) {
            return nullptr;
        }

        auto &receivedMessages = backend_->receivedMessages();
        for (auto &msgData : receivedMessages) {
            if (msgData.msg && msgData.msg->type == T::Type) {
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
        if (!backend_) {
            return results;
        }
        auto &receivedMessages = backend_->receivedMessages();
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
