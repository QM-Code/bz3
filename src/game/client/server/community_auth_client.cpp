#include "client/server/community_auth_client.hpp"

#include <curl/curl.h>
#include "common/json.hpp"

#include "common/curl_global.hpp"

namespace {
size_t CurlWriteToString(char *ptr, size_t size, size_t nmemb, void *userdata) {
    auto *buffer = static_cast<std::string *>(userdata);
    const size_t total = size * nmemb;
    buffer->append(ptr, total);
    return total;
}

std::string normalizedHost(const std::string &host) {
    if (host.empty()) {
        return {};
    }
    std::string trimmed = host;
    while (!trimmed.empty() && trimmed.back() == '/') {
        trimmed.pop_back();
    }
    return trimmed;
}

bool performGet(const std::string &url, std::string &outBody) {
    if (!karma::net::EnsureCurlGlobalInit()) {
        return false;
    }
    CURL *curlHandle = curl_easy_init();
    if (!curlHandle) {
        return false;
    }
    curl_easy_setopt(curlHandle, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curlHandle, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curlHandle, CURLOPT_TIMEOUT, 5L);
    curl_easy_setopt(curlHandle, CURLOPT_WRITEFUNCTION, CurlWriteToString);
    curl_easy_setopt(curlHandle, CURLOPT_WRITEDATA, &outBody);

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

bool performPost(const std::string &url, const std::string &formBody, std::string &outBody) {
    if (!karma::net::EnsureCurlGlobalInit()) {
        return false;
    }
    CURL *curlHandle = curl_easy_init();
    if (!curlHandle) {
        return false;
    }

    curl_easy_setopt(curlHandle, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curlHandle, CURLOPT_POST, 1L);
    curl_easy_setopt(curlHandle, CURLOPT_POSTFIELDS, formBody.c_str());
    curl_easy_setopt(curlHandle, CURLOPT_POSTFIELDSIZE, static_cast<long>(formBody.size()));
    curl_easy_setopt(curlHandle, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curlHandle, CURLOPT_TIMEOUT, 5L);
    curl_easy_setopt(curlHandle, CURLOPT_WRITEFUNCTION, CurlWriteToString);
    curl_easy_setopt(curlHandle, CURLOPT_WRITEDATA, &outBody);
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

std::string urlEncode(CURL *curlHandle, const std::string &value) {
    char *escaped = curl_easy_escape(curlHandle, value.c_str(), static_cast<int>(value.size()));
    if (!escaped) {
        return {};
    }
    std::string result(escaped);
    curl_free(escaped);
    return result;
}
} // namespace

CommunityAuthClient::CommunityAuthClient() = default;

CommunityAuthClient::~CommunityAuthClient() {
    stopWorker();
}

void CommunityAuthClient::requestUserRegistered(const std::string &host, const std::string &username) {
    Request request;
    request.type = RequestType::UserRegistered;
    request.host = host;
    request.username = username;
    startWorker();
    {
        std::lock_guard<std::mutex> lock(mutex);
        requests.push_back(std::move(request));
    }
    cv.notify_one();
}

void CommunityAuthClient::requestAuth(const std::string &host,
                                      const std::string &username,
                                      const std::string &passhash,
                                      const std::string &worldName) {
    Request request;
    request.type = RequestType::Auth;
    request.host = host;
    request.username = username;
    request.passhash = passhash;
    request.worldName = worldName;
    startWorker();
    {
        std::lock_guard<std::mutex> lock(mutex);
        requests.push_back(std::move(request));
    }
    cv.notify_one();
}

std::optional<CommunityAuthClient::Response> CommunityAuthClient::consumeResponse() {
    std::lock_guard<std::mutex> lock(mutex);
    if (responses.empty()) {
        return std::nullopt;
    }
    Response response = std::move(responses.front());
    responses.pop_front();
    return response;
}

void CommunityAuthClient::startWorker() {
    if (worker.joinable()) {
        return;
    }
    stopRequested = false;
    worker = std::thread(&CommunityAuthClient::workerProc, this);
}

void CommunityAuthClient::stopWorker() {
    {
        std::lock_guard<std::mutex> lock(mutex);
        stopRequested = true;
        requests.clear();
        responses.clear();
    }
    cv.notify_all();
    if (worker.joinable()) {
        worker.join();
    }
}

void CommunityAuthClient::workerProc() {
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

        Response response;
        response.type = request.type;
        response.host = request.host;
        response.username = request.username;

        std::string hostBase = normalizedHost(request.host);
        if (hostBase.empty()) {
            response.ok = false;
            response.error = "invalid_host";
    } else if (request.type == RequestType::UserRegistered) {
            if (!karma::net::EnsureCurlGlobalInit()) {
                response.ok = false;
                response.error = "curl_init_failed";
            } else {
                CURL *curlHandle = curl_easy_init();
            if (!curlHandle) {
                response.ok = false;
                response.error = "curl_init_failed";
            } else {
                std::string encodedUser = urlEncode(curlHandle, request.username);
                curl_easy_cleanup(curlHandle);
                std::string url = hostBase + "/api/user_registered?username=" + encodedUser;
                std::string body;
                if (!performGet(url, body)) {
                    response.ok = false;
                    response.error = "request_failed";
                } else {
                    try {
                        auto jsonData = karma::json::Parse(body);
                        response.ok = jsonData.value("ok", false);
                        response.communityName = jsonData.value("community_name", std::string{});
                        response.registered = jsonData.value("registered", false);
                        response.salt = jsonData.value("salt", std::string{});
                        response.locked = jsonData.value("locked", false);
                        response.deleted = jsonData.value("deleted", false);
                        if (!response.ok) {
                            response.error = jsonData.value("error", std::string{});
                        }
                    } catch (const std::exception &ex) {
                        response.ok = false;
                        response.error = ex.what();
                    }
                }
            }
            }
        } else {
            if (!karma::net::EnsureCurlGlobalInit()) {
                response.ok = false;
                response.error = "curl_init_failed";
            } else {
                CURL *curlHandle = curl_easy_init();
            if (!curlHandle) {
                response.ok = false;
                response.error = "curl_init_failed";
            } else {
                std::string encodedUser = urlEncode(curlHandle, request.username);
                std::string encodedHash = urlEncode(curlHandle, request.passhash);
                std::string encodedWorld = request.worldName.empty()
                    ? std::string{}
                    : urlEncode(curlHandle, request.worldName);
                curl_easy_cleanup(curlHandle);
                std::string url = hostBase + "/api/auth";
                std::string formBody = "username=" + encodedUser + "&passhash=" + encodedHash;
                if (!encodedWorld.empty()) {
                    formBody += "&world=" + encodedWorld;
                }
                std::string body;
                if (!performPost(url, formBody, body)) {
                    response.ok = false;
                    response.error = "request_failed";
                } else {
                    try {
                        auto jsonData = karma::json::Parse(body);
                        response.ok = jsonData.value("ok", false);
                        response.error = jsonData.value("error", std::string{});
                        response.communityAdmin = jsonData.value("community_admin", false);
                        response.localAdmin = jsonData.value("local_admin", false);
                    } catch (const std::exception &ex) {
                        response.ok = false;
                        response.error = ex.what();
                    }
                }
            }
            }
        }

        {
            std::lock_guard<std::mutex> lock(mutex);
            responses.push_back(std::move(response));
        }
    }
}
