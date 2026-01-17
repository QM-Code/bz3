#include "client/server/server_list_fetcher.hpp"

#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include "common/curl_global.hpp"
#include "common/data_path_resolver.hpp"
#include "spdlog/spdlog.h"

#include <sstream>

namespace {
size_t CurlWriteCallback(char *ptr, size_t size, size_t nmemb, void *userdata) {
    size_t total = size * nmemb;
    auto *buffer = static_cast<std::string *>(userdata);
    buffer->append(ptr, total);
    return total;
}

int parseIntegerField(const nlohmann::json &object, const char *key) {
    if (!object.contains(key)) {
        return -1;
    }

    const auto &value = object.at(key);
    try {
        if (value.is_number_integer()) {
            return value.get<int>();
        }
        if (value.is_string()) {
            return std::stoi(value.get<std::string>());
        }
    } catch (...) {
        return -1;
    }

    return -1;
}

std::string configuredServerPortString() {
    if (const auto label = bz::data::ConfigValueString("network.ServerPort")) {
        return *label;
    }
    if (const auto numeric = bz::data::ConfigValueUInt16("network.ServerPort")) {
        return std::to_string(*numeric);
    }
    return std::string("0");
}

uint16_t configuredServerPortValue() {
    if (const auto numeric = bz::data::ConfigValueUInt16("network.ServerPort")) {
        return *numeric;
    }
    if (const auto label = bz::data::ConfigValueString("network.ServerPort")) {
        try {
            return static_cast<uint16_t>(std::stoul(*label));
        } catch (...) {
            return 0;
        }
    }
    return 0;
}

std::string buildServersUrl(const std::string &baseHost) {
    if (baseHost.empty()) {
        return {};
    }

    std::string normalized = baseHost;
    // Trim trailing slashes so we can append predictable paths.
    while (!normalized.empty() && normalized.back() == '/') {
        normalized.pop_back();
    }

    return normalized + "/api/servers";
}
}

ServerListFetcher::ServerListFetcher(std::vector<ClientServerListSource> sources)
    : sources(std::move(sources)) {
    if (!this->sources.empty()) {
        if (bz::net::EnsureCurlGlobalInit()) {
            curlInitialized = true;
        } else {
            spdlog::warn("ServerListFetcher: Failed to initialize cURL");
        }
    }
}

ServerListFetcher::~ServerListFetcher() {
    if (worker.joinable()) {
        worker.join();
    }
}

void ServerListFetcher::requestRefresh() {
    if (sources.empty() || !curlInitialized) {
        return;
    }

    bool expected = false;
    if (!fetching.compare_exchange_strong(expected, true)) {
        return;
    }

    if (worker.joinable()) {
        worker.join();
    }

    worker = std::thread(&ServerListFetcher::workerProc, this);
}

std::vector<ServerListFetcher::ServerRecord> ServerListFetcher::getServers() const {
    std::lock_guard<std::mutex> lock(recordsMutex);
    return records;
}

std::vector<ServerListFetcher::SourceStatus> ServerListFetcher::getSourceStatuses() const {
    std::lock_guard<std::mutex> lock(recordsMutex);
    return sourceStatuses;
}

std::size_t ServerListFetcher::getGeneration() const {
    return generation.load();
}

bool ServerListFetcher::isFetching() const {
    return fetching.load();
}

void ServerListFetcher::workerProc() {
    auto newRecords = fetchOnce();
    {
        std::lock_guard<std::mutex> lock(recordsMutex);
        records = std::move(newRecords);
        generation++;
    }
    fetching.store(false);
}

std::vector<ServerListFetcher::ServerRecord> ServerListFetcher::fetchOnce() {
    std::vector<ServerRecord> combined;
    combined.reserve(16);
    std::vector<SourceStatus> statuses;
    statuses.reserve(sources.size());

    for (const auto &source : sources) {
        const std::string listUrl = buildServersUrl(source.host);
        if (listUrl.empty()) {
            spdlog::warn("ServerListFetcher: Skipping source with empty host");
            continue;
        }

        SourceStatus status;
        status.sourceHost = source.host;

        std::string responseBody;
        if (!fetchUrl(listUrl, responseBody)) {
            status.ok = false;
            status.hasData = false;
            status.error = "request_failed";
            statuses.push_back(std::move(status));
            continue;
        }

        auto parsed = parseResponse(source, responseBody, &status);
        combined.insert(combined.end(), parsed.begin(), parsed.end());
        statuses.push_back(std::move(status));
    }

    {
        std::lock_guard<std::mutex> lock(recordsMutex);
        sourceStatuses = std::move(statuses);
    }

    return combined;
}

bool ServerListFetcher::fetchUrl(const std::string &url, std::string &outBody) {
    CURL *curlHandle = curl_easy_init();
    if (!curlHandle) {
        spdlog::warn("ServerListFetcher: curl_easy_init failed");
        return false;
    }

    curl_easy_setopt(curlHandle, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curlHandle, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curlHandle, CURLOPT_TIMEOUT, 5L);
    curl_easy_setopt(curlHandle, CURLOPT_WRITEFUNCTION, CurlWriteCallback);
    curl_easy_setopt(curlHandle, CURLOPT_WRITEDATA, &outBody);

    CURLcode result = curl_easy_perform(curlHandle);
    if (result != CURLE_OK) {
        spdlog::warn("ServerListFetcher: Request to {} failed: {}", url, curl_easy_strerror(result));
        curl_easy_cleanup(curlHandle);
        return false;
    }

    long status = 0;
    curl_easy_getinfo(curlHandle, CURLINFO_RESPONSE_CODE, &status);
    curl_easy_cleanup(curlHandle);

    if (status < 200 || status >= 300) {
        spdlog::warn("ServerListFetcher: {} returned HTTP status {}", url, status);
        return false;
    }

    return true;
}

std::vector<ServerListFetcher::ServerRecord> ServerListFetcher::parseResponse(
    const ClientServerListSource &source,
    const std::string &body,
    SourceStatus *statusOut) {
    std::vector<ServerRecord> records;

    try {
        nlohmann::json jsonData = nlohmann::json::parse(body);
        std::string listName;
        if (auto nameIt = jsonData.find("name"); nameIt != jsonData.end() && nameIt->is_string()) {
            listName = nameIt->get<std::string>();
        }

        std::string communityName;
        int activeCount = -1;
        int inactiveCount = -1;
        if (auto commIt = jsonData.find("community_name"); commIt != jsonData.end() && commIt->is_string()) {
            communityName = commIt->get<std::string>();
        }
        if (auto activeIt = jsonData.find("active_count"); activeIt != jsonData.end() && activeIt->is_number_integer()) {
            activeCount = activeIt->get<int>();
        }
        if (auto inactiveIt = jsonData.find("inactive_count"); inactiveIt != jsonData.end() && inactiveIt->is_number_integer()) {
            inactiveCount = inactiveIt->get<int>();
        }

        if (!jsonData.contains("servers") || !jsonData["servers"].is_array()) {
            spdlog::warn("ServerListFetcher: Server list from {} missing 'servers' array", source.host);
            if (statusOut) {
                statusOut->ok = false;
                statusOut->hasData = true;
                statusOut->error = "invalid_response";
            }
            return records;
        }

        std::string sourceDisplayName = source.name.empty() ? source.host : source.name;
        if (!communityName.empty()) {
            sourceDisplayName = communityName;
        } else if (!listName.empty()) {
            sourceDisplayName = listName;
        }

        if (statusOut) {
            statusOut->communityName = communityName.empty() ? sourceDisplayName : communityName;
            statusOut->activeCount = activeCount;
            statusOut->inactiveCount = inactiveCount;
            statusOut->ok = true;
            statusOut->hasData = true;
        }

        for (const auto &server : jsonData["servers"]) {
            if (!server.is_object()) {
                continue;
            }

            if (!server.contains("host")) {
                continue;
            }

            ServerRecord record;
            record.sourceName = sourceDisplayName;
            record.sourceHost = source.host;
            record.communityName = communityName;
            record.activeCount = activeCount;
            record.inactiveCount = inactiveCount;
            record.host = server.value("host", "");
            std::string portString = server.value("port", configuredServerPortString());

            try {
                record.port = static_cast<uint16_t>(std::stoi(portString));
            } catch (...) {
                record.port = configuredServerPortValue();
            }

            record.name = server.value("name", record.host);
            record.gameMode = server.value("game_mode", std::string{});
            record.maxPlayers = parseIntegerField(server, "max_players");
            record.activePlayers = parseIntegerField(server, "active_players");
            if (record.activePlayers < 0) {
                record.activePlayers = parseIntegerField(server, "num_players");
            }
            if (server.contains("screenshot_id") && server["screenshot_id"].is_string()) {
                record.screenshotId = server["screenshot_id"].get<std::string>();
            }
            record.description = server.value("description", server.value("descrpition", std::string{}));
            record.flags.clear();
            if (server.contains("flags") && server["flags"].is_array()) {
                for (const auto &flagValue : server["flags"]) {
                    if (flagValue.is_string()) {
                        record.flags.push_back(flagValue.get<std::string>());
                    }
                }
            }

            if (record.host.empty()) {
                continue;
            }

            records.push_back(record);
        }
    } catch (const std::exception &ex) {
        spdlog::warn("ServerListFetcher: Failed to parse response from {}: {}", source.host, ex.what());
    }

    return records;
}
