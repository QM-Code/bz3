#pragma once

#include <condition_variable>
#include <deque>
#include <mutex>
#include <string>
#include <thread>

class HeartbeatClient {
public:
    HeartbeatClient();
    ~HeartbeatClient();

    void requestHeartbeat(const std::string &communityUrl,
                          const std::string &serverAddress,
                          int players,
                          int maxPlayers);

private:
    struct Request {
        std::string communityUrl;
        std::string serverAddress;
        int players = 0;
        int maxPlayers = 0;
    };

    void startWorker();
    void stopWorker();
    void workerProc();

    std::deque<Request> requests;
    std::mutex mutex;
    std::condition_variable cv;
    std::thread worker;
    bool stopRequested = false;
};
