#pragma once

#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

class CommunityAuthClient {
public:
    enum class RequestType {
        UserRegistered,
        Auth
    };

    struct Response {
        RequestType type;
        bool ok = false;
        std::string host;
        std::string username;
        std::string error;
        std::string communityName;
        bool registered = false;
        bool locked = false;
        bool deleted = false;
        std::string salt;
        bool communityAdmin = false;
        bool localAdmin = false;
    };

    CommunityAuthClient();
    ~CommunityAuthClient();

    void requestUserRegistered(const std::string &host, const std::string &username);
    void requestAuth(const std::string &host,
                     const std::string &username,
                     const std::string &passhash,
                     const std::string &worldName);
    std::optional<Response> consumeResponse();

private:
    struct Request {
        RequestType type;
        std::string host;
        std::string username;
        std::string passhash;
        std::string worldName;
    };

    void startWorker();
    void stopWorker();
    void workerProc();

    std::deque<Request> requests;
    std::deque<Response> responses;
    std::mutex mutex;
    std::condition_variable cv;
    std::thread worker;
    bool stopRequested = false;
};
