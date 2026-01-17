#include "server/heartbeat_client.hpp"

#include <curl/curl.h>
#include <spdlog/spdlog.h>

#include "common/curl_global.hpp"

namespace {
std::string trimTrailingSlash(std::string value) {
    while (!value.empty() && value.back() == '/') {
        value.pop_back();
    }
    return value;
}

std::string urlEncode(CURL *curlHandle, const std::string &value) {
    char *escaped = curl_easy_escape(curlHandle, value.c_str(), static_cast<int>(value.size()));
    if (!escaped) {
        return {};
    }
    std::string result(escaped);
    curl_free(escaped);
    return result;
}

bool performGet(const std::string &url) {
    if (!bz::net::EnsureCurlGlobalInit()) {
        return false;
    }
    CURL *curlHandle = curl_easy_init();
    if (!curlHandle) {
        return false;
    }
    curl_easy_setopt(curlHandle, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curlHandle, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curlHandle, CURLOPT_TIMEOUT, 5L);
    CURLcode result = curl_easy_perform(curlHandle);
    long status = 0;
    if (result == CURLE_OK) {
        curl_easy_getinfo(curlHandle, CURLINFO_RESPONSE_CODE, &status);
    }
    curl_easy_cleanup(curlHandle);

    if (result != CURLE_OK || status < 200 || status >= 300) {
        return false;
    }
    return true;
}
} // namespace

HeartbeatClient::HeartbeatClient() = default;

HeartbeatClient::~HeartbeatClient() {
    stopWorker();
}

void HeartbeatClient::requestHeartbeat(const std::string &communityUrl,
                                       const std::string &serverAddress,
                                       int players,
                                       int maxPlayers) {
    if (communityUrl.empty() || serverAddress.empty()) {
        return;
    }

    Request request;
    request.communityUrl = communityUrl;
    request.serverAddress = serverAddress;
    request.players = players;
    request.maxPlayers = maxPlayers;

    startWorker();
    {
        std::lock_guard<std::mutex> lock(mutex);
        requests.clear();
        requests.push_back(std::move(request));
    }
    cv.notify_one();
}

void HeartbeatClient::startWorker() {
    if (worker.joinable()) {
        return;
    }
    stopRequested = false;
    worker = std::thread(&HeartbeatClient::workerProc, this);
}

void HeartbeatClient::stopWorker() {
    {
        std::lock_guard<std::mutex> lock(mutex);
        stopRequested = true;
        requests.clear();
    }
    cv.notify_all();
    if (worker.joinable()) {
        worker.join();
    }
}

void HeartbeatClient::workerProc() {
    while (true) {
        Request request;
        {
            std::unique_lock<std::mutex> lock(mutex);
            cv.wait(lock, [&]() { return stopRequested || !requests.empty(); });
            if (stopRequested) {
                return;
            }
            request = std::move(requests.front());
            requests.pop_front();
        }

        if (!bz::net::EnsureCurlGlobalInit()) {
            spdlog::warn("HeartbeatClient: Failed to initialize cURL");
            continue;
        }

        CURL *curlHandle = curl_easy_init();
        if (!curlHandle) {
            spdlog::warn("HeartbeatClient: curl_easy_init failed");
            continue;
        }

        const std::string baseUrl = trimTrailingSlash(request.communityUrl);
        const std::string encodedServer = urlEncode(curlHandle, request.serverAddress);
        const std::string encodedPlayers = urlEncode(curlHandle, std::to_string(request.players));
        const std::string encodedMax = urlEncode(curlHandle, std::to_string(request.maxPlayers));
        curl_easy_cleanup(curlHandle);

        if (encodedServer.empty()) {
            spdlog::warn("HeartbeatClient: Failed to encode server address");
            continue;
        }

        std::string url = baseUrl + "/api/heartbeat?server=" + encodedServer +
            "&players=" + encodedPlayers +
            "&max=" + encodedMax;

        if (!performGet(url)) {
            spdlog::warn("HeartbeatClient: Heartbeat request failed for {}", baseUrl);
        }
    }
}
