#include "client/server/community_browser_controller.hpp"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "client/server/password_hash.hpp"
#include "client/server/server_connector.hpp"
#include "common/curl_global.hpp"
#include "common/config_helpers.hpp"
#include "spdlog/spdlog.h"
#include <curl/curl.h>
#include "common/json.hpp"

namespace {
std::string trimCopy(const std::string &value) {
    auto begin = std::find_if_not(value.begin(), value.end(), [](unsigned char ch) {
        return std::isspace(ch) != 0;
    });
    auto end = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char ch) {
        return std::isspace(ch) != 0;
    }).base();

    if (begin >= end) {
        return {};
    }

    return std::string(begin, end);
}

bool equalsIgnoreCase(const std::string &lhs, const std::string &rhs) {
    if (lhs.size() != rhs.size()) {
        return false;
    }

    for (std::size_t i = 0; i < lhs.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(lhs[i])) != std::tolower(static_cast<unsigned char>(rhs[i]))) {
            return false;
        }
    }

    return true;
}

bool isLanToken(const std::string &value) {
    if (value.empty()) {
        return false;
    }

    std::string trimmed = trimCopy(value);
    return equalsIgnoreCase(trimmed, "LAN") || equalsIgnoreCase(trimmed, "Local Area Network");
}

uint16_t configuredServerPort() {
    return bz::config::ReadUInt16Config({"network.ServerPort"}, 0);
}

uint16_t applyPortFallback(uint16_t candidate) {
    if (candidate != 0) {
        return candidate;
    }
    return configuredServerPort();
}

size_t CurlWriteCallback(char *ptr, size_t size, size_t nmemb, void *userdata) {
    size_t total = size * nmemb;
    auto *buffer = static_cast<std::string *>(userdata);
    buffer->append(ptr, total);
    return total;
}

bool fetchUrl(const std::string &url, std::string &outBody, long *statusOut) {
    CURL *curlHandle = curl_easy_init();
    if (!curlHandle) {
        spdlog::warn("CommunityBrowserController: curl_easy_init failed");
        return false;
    }

    curl_easy_setopt(curlHandle, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curlHandle, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curlHandle, CURLOPT_TIMEOUT, 5L);
    curl_easy_setopt(curlHandle, CURLOPT_WRITEFUNCTION, CurlWriteCallback);
    curl_easy_setopt(curlHandle, CURLOPT_WRITEDATA, &outBody);

    CURLcode result = curl_easy_perform(curlHandle);
    if (result != CURLE_OK) {
        spdlog::warn("CommunityBrowserController: Request to {} failed: {}", url, curl_easy_strerror(result));
        curl_easy_cleanup(curlHandle);
        return false;
    }

    long status = 0;
    curl_easy_getinfo(curlHandle, CURLINFO_RESPONSE_CODE, &status);
    curl_easy_cleanup(curlHandle);
    if (statusOut) {
        *statusOut = status;
    }

    if (status < 200 || status >= 300) {
        spdlog::warn("CommunityBrowserController: {} returned HTTP status {}", url, status);
        return false;
    }

    return true;
}

std::string urlEncode(const std::string &value) {
    std::ostringstream encoded;
    encoded << std::uppercase << std::hex << std::setfill('0');
    for (unsigned char ch : value) {
        if ((ch >= 'a' && ch <= 'z') ||
            (ch >= 'A' && ch <= 'Z') ||
            (ch >= '0' && ch <= '9') ||
            ch == '-' || ch == '_' || ch == '.' || ch == '~') {
            encoded << ch;
        } else if (ch == ' ') {
            encoded << "%20";
        } else {
            encoded << '%' << std::setw(2) << static_cast<int>(ch);
        }
    }
    return encoded.str();
}

std::string normalizedCommunityHost(const std::string &host) {
    if (host.empty()) {
        return {};
    }
    std::string normalized = host;
    while (!normalized.empty() && normalized.back() == '/') {
        normalized.pop_back();
    }
    if (normalized.rfind("http://", 0) == 0 || normalized.rfind("https://", 0) == 0) {
        return normalized;
    }
    return "http://" + normalized;
}

std::string buildServerDetailsUrl(const std::string &host, const std::string &serverCode) {
    if (host.empty() || serverCode.empty()) {
        return {};
    }
    const std::string base = normalizedCommunityHost(host);
    if (base.empty()) {
        return {};
    }
    return base + "/api/server/" + urlEncode(serverCode);
}

struct ParsedCommunityHost {
    std::string baseUrl;
    std::string displayHost;
};

bool parseCommunityUrl(std::string_view input, ParsedCommunityHost &out) {
    std::string text = trimCopy(std::string(input));
    if (text.rfind("http://", 0) != 0 && text.rfind("https://", 0) != 0) {
        return false;
    }
    std::string scheme = text.rfind("https://", 0) == 0 ? "https://" : "http://";
    std::string rest = text.substr(scheme.size());
    if (rest.empty()) {
        return false;
    }
    const std::size_t slash = rest.find('/');
    std::string authority = slash == std::string::npos ? rest : rest.substr(0, slash);
    if (authority.empty()) {
        return false;
    }
    std::string host = authority;
    std::string port;
    const std::size_t colon = authority.rfind(':');
    if (colon != std::string::npos) {
        host = authority.substr(0, colon);
        port = authority.substr(colon + 1);
        if (port.empty() || port.find_first_not_of("0123456789") != std::string::npos) {
            return false;
        }
    }
    if (host.empty()) {
        return false;
    }
    if (host != "localhost" && host.find('.') == std::string::npos) {
        return false;
    }
    out.displayHost = authority;
    out.baseUrl = scheme + authority;
    return true;
}

bool fetchCommunityInfoOk(const std::string &baseUrl) {
    std::string body;
    long status = 0;
    if (!fetchUrl(baseUrl + "/api/info", body, &status)) {
        return false;
    }
    try {
        auto json = bz::json::Parse(body);
        return json.is_object();
    } catch (...) {
        return false;
    }
}
}

CommunityBrowserController::CommunityBrowserController(ClientEngine &engine,
                                                       ClientConfig &clientConfig,
                                                       const std::string &configPath,
                                                       ServerConnector &connector)
    : engine(engine),
      browser(engine.ui->console()),
      clientConfig(clientConfig),
      clientConfigPath(configPath),
      connector(connector) {
    curlReady = bz::net::EnsureCurlGlobalInit();
    if (!curlReady) {
        spdlog::warn("CommunityBrowserController: Failed to initialize cURL");
    }
    refreshGuiServerListOptions();
    rebuildServerListFetcher();

    browser.show({});
    browser.setUserConfigPath(clientConfigPath);
    triggerFullRefresh();
}

CommunityBrowserController::~CommunityBrowserController() {
    if (serverDetailsRequest && serverDetailsRequest->worker.joinable()) {
        serverDetailsRequest->worker.join();
    }
}

void CommunityBrowserController::triggerFullRefresh() {
    const auto nowSteady = SteadyClock::now();
    bool lanActive = isLanSelected();
    bool issuedRequest = false;

    if (lanActive) {
        discovery.startScan();
        issuedRequest = true;
    }

    if (serverListFetcher) {
        serverListFetcher->requestRefresh();
        issuedRequest = true;
    }

    if (!issuedRequest) {
        browser.setStatus("No server sources configured. Add a server list or enable Local Area Network.", true);
        browser.setScanning(false);
        return;
    }

    std::string selectionLabel = "selected server list";
    if (!lanActive) {
        if (const auto *source = getSelectedRemoteSource()) {
            selectionLabel = resolveDisplayNameForSource(*source);
        }
    }

    if (lanActive && serverListFetcher) {
        browser.setCommunityStatus("Searching local network and fetching the selected server list...",
                                   ui::MessageTone::Pending);
    } else if (lanActive) {
        browser.setCommunityStatus("Searching local network for servers...",
                                   ui::MessageTone::Pending);
    } else {
        browser.setCommunityStatus("Fetching " + selectionLabel + "...",
                                   ui::MessageTone::Pending);
    }

    browser.setScanning(issuedRequest);
}

void CommunityBrowserController::rebuildEntries() {
    const auto &servers = discovery.getServers();
    const bool lanViewActive = isLanSelected();

    auto makeKey = [](const std::string &host, uint16_t port) {
        return host + ":" + std::to_string(port);
    };

    auto buildRemoteDescription = [](const ServerListFetcher::ServerRecord &record) {
        std::string description = record.sourceName.empty() ? "Public list" : record.sourceName;
        std::string details;
        if (record.activePlayers >= 0) {
            details = std::to_string(record.activePlayers);
            if (record.maxPlayers >= 0) {
                details += "/" + std::to_string(record.maxPlayers);
            }
            details += " players";
        }
        if (!record.gameMode.empty()) {
            if (!details.empty()) {
                details += " · ";
            }
            details += record.gameMode;
        }
        if (!details.empty()) {
            if (!description.empty()) {
                description += " — ";
            }
            description += details;
        }
        return description;
    };

    std::vector<ui::CommunityBrowserEntry> entries;
    entries.reserve(servers.size() + cachedRemoteServers.size());
    std::unordered_set<std::string> seen;
    seen.reserve(entries.capacity() > 0 ? entries.capacity() : 1);

    if (lanViewActive) {
        for (const auto &serverInfo : servers) {
            if (serverInfo.host.empty()) {
                continue;
            }
            auto key = makeKey(serverInfo.host, serverInfo.port);
            if (!seen.insert(key).second) {
                continue;
            }
            ui::CommunityBrowserEntry entry;
            const std::string addressLabel = serverInfo.host + ":" + std::to_string(serverInfo.port);
            entry.label = addressLabel;
            entry.host = serverInfo.host;
            entry.port = serverInfo.port;
            entry.description = serverInfo.name.empty() ? "Discovered via broadcast" : serverInfo.name;
            if (!serverInfo.world.empty()) {
                entry.description += " — " + serverInfo.world;
            }
            entry.displayHost = serverInfo.displayHost.empty() ? serverInfo.host : serverInfo.displayHost;
            entry.longDescription = serverInfo.world.empty()
                ? std::string("Discovered via LAN broadcast.")
                : (std::string("World: ") + serverInfo.world);
            entry.flags.clear();
            entry.activePlayers = -1;
            entry.maxPlayers = -1;
            entry.gameMode.clear();
            entries.push_back(std::move(entry));
        }
    }

    for (const auto &record : cachedRemoteServers) {
        if (record.host.empty()) {
            continue;
        }
        uint16_t recordPort = applyPortFallback(record.port);
        auto key = makeKey(record.host, recordPort);
        if (!seen.insert(key).second) {
            continue;
        }
        ui::CommunityBrowserEntry entry;
        entry.label = record.name.empty() ? record.host : record.name;
        entry.host = record.host;
        entry.port = recordPort;
        entry.description = buildRemoteDescription(record);
        entry.displayHost = record.host;
        if (!record.overview.empty()) {
            entry.description = record.overview;
        }
        entry.longDescription = record.detailDescription;
        entry.code = record.code;
        entry.flags = record.flags;
        entry.activePlayers = record.activePlayers;
        entry.maxPlayers = record.maxPlayers;
        entry.gameMode = record.gameMode;
        entry.screenshotId = record.screenshotId;
        entry.sourceHost = record.sourceHost;
        entry.worldName = record.name;
        entries.push_back(std::move(entry));
    }

    lastGuiEntries = entries;
    browser.setEntries(lastGuiEntries);
    if (!entries.empty()) {
        browser.setStatus("Select a server to connect.", false);
    }
}

void CommunityBrowserController::update() {
    while (auto response = authClient.consumeResponse()) {
        handleAuthResponse(*response);
    }

    if (auto listSelection = browser.consumeListSelection()) {
        handleServerListSelection(*listSelection);
    }

    if (auto newList = browser.consumeNewListRequest()) {
        handleServerListAddition(*newList);
    }

    if (auto deleteHost = browser.consumeDeleteListRequest()) {
        handleServerListDeletion(*deleteHost);
    }

    if (browser.consumeRefreshRequest()) {
        triggerFullRefresh();
    }

    discovery.update();
    bool remoteFetchingActive = serverListFetcher && serverListFetcher->isFetching();
    browser.setScanning(discovery.isScanning() || remoteFetchingActive);

    bool entriesDirty = false;
    auto discoveryVersion = discovery.getGeneration();
    if (discoveryVersion != lastDiscoveryVersion) {
        lastDiscoveryVersion = discoveryVersion;
        entriesDirty = true;
    }

    if (serverListFetcher) {
        std::size_t remoteGeneration = serverListFetcher->getGeneration();
        if (remoteGeneration != lastServerListGeneration) {
            cachedRemoteServers = serverListFetcher->getServers();
            cachedSourceStatuses = serverListFetcher->getSourceStatuses();
            lastServerListGeneration = remoteGeneration;
            entriesDirty = true;
            updateServerListDisplayNamesFromCache();
        }
    }

    if (entriesDirty) {
        rebuildEntries();
    }

    if (pendingAddRequest && pendingAddRequest->done.load()) {
        if (pendingAddRequest->worker.joinable()) {
            pendingAddRequest->worker.join();
        }
        if (pendingAddRequest->ok) {
            commitServerListAddition(pendingAddRequest->baseUrl);
        } else {
            browser.showErrorDialog("Failed to connect to " + pendingAddRequest->displayHost);
        }
        pendingAddRequest.reset();
    }

    if (remoteFetchingActive && serverListFetcher && !isLanSelected()) {
        std::string selectionLabel = "selected server list";
        const ClientServerListSource *source = getSelectedRemoteSource();
        if (source) {
            selectionLabel = resolveDisplayNameForSource(*source);
        }
        bool hasStatus = false;
        if (source && !source->host.empty()) {
            for (const auto &status : cachedSourceStatuses) {
                if (status.sourceHost == source->host) {
                    hasStatus = true;
                    break;
                }
            }
        }
        if (source && !source->host.empty() && !hasStatus) {
            browser.setCommunityStatus("Connecting to " + selectionLabel + " at " + source->host + "...",
                                       ui::MessageTone::Pending);
        } else {
            browser.setCommunityStatus("Fetching " + selectionLabel + "...",
                                       ui::MessageTone::Pending);
        }
    } else if (serverListFetcher && !isLanSelected()) {
        std::string statusText;
        ui::MessageTone tone = ui::MessageTone::Notice;
        const auto *source = getSelectedRemoteSource();
        if (source && !source->host.empty()) {
            for (const auto &status : cachedSourceStatuses) {
                if (status.sourceHost != source->host) {
                    continue;
                }
                if (!status.ok) {
                    statusText = "Failed to reach community server";
                    if (!source->host.empty()) {
                        statusText += " (" + source->host + ")";
                    }
                    tone = ui::MessageTone::Error;
                } else if (status.activeCount == 0) {
                    statusText = "Community currently has no active servers";
                    if (status.inactiveCount >= 0) {
                        statusText += " (" + std::to_string(status.inactiveCount) + " inactive)";
                    }
                }
                break;
            }
        }
        browser.setCommunityStatus(statusText, tone);
    } else if (isLanSelected() && discovery.isScanning()) {
        browser.setCommunityStatus("Searching local network for servers...",
                                   ui::MessageTone::Pending);
    } else {
        browser.setCommunityStatus(std::string{}, ui::MessageTone::Notice);
    }

    const auto &servers = discovery.getServers();
    bool lanEmpty = servers.empty();
    bool remoteEmpty = cachedRemoteServers.empty();

    if (auto selection = browser.consumeSelection()) {
        handleJoinSelection(*selection);
    }

    const ClientServerListSource *selectedRemoteSource = getSelectedRemoteSource();
    std::string remoteLabel = selectedRemoteSource ? resolveDisplayNameForSource(*selectedRemoteSource) : "selected server list";

    if (lanEmpty && remoteEmpty) {
        if (discovery.isScanning() && isLanSelected()) {
            browser.setStatus(std::string{}, false);
        browser.setCommunityStatus("Searching local network for servers...",
                                       ui::MessageTone::Pending);
        } else if (remoteFetchingActive && serverListFetcher) {
            browser.setStatus(std::string{}, false);
        } else if (isLanSelected()) {
            browser.setStatus(std::string{}, false);
        browser.setCommunityStatus("No LAN servers found. Start one locally or refresh.",
                                       ui::MessageTone::Notice);
        } else if (serverListFetcher) {
            browser.setStatus(std::string{}, false);
        } else {
            browser.setStatus("No server sources configured. Add a server list or enable Local Area Network.", true);
        }
    }

    updateCommunityDetails();

    if (serverDetailsRequest && serverDetailsRequest->done.load()) {
        if (serverDetailsRequest->worker.joinable()) {
            serverDetailsRequest->worker.join();
        }
        if (serverDetailsRequest->ok && !serverDetailsRequest->description.empty()) {
            serverDescriptionCache[serverDetailsRequest->key] = serverDetailsRequest->description;
            serverDescriptionErrors.erase(serverDetailsRequest->key);
            for (auto &record : cachedRemoteServers) {
                if (makeServerDetailsKey(record) == serverDetailsRequest->key) {
                    record.detailDescription = serverDetailsRequest->description;
                    if (!serverDetailsRequest->detailName.empty()) {
                        record.name = serverDetailsRequest->detailName;
                    }
                }
            }
            rebuildEntries();
        } else {
            serverDescriptionFailed.insert(serverDetailsRequest->key);
            if (!serverDetailsRequest->description.empty()) {
                serverDescriptionErrors[serverDetailsRequest->key] = serverDetailsRequest->description;
            }
        }
        serverDetailsRequest.reset();
    }

    auto selectedEntry = browser.getSelectedEntry();
    if (!selectedEntry || selectedEntry->sourceHost.empty()) {
        browser.setServerDescriptionLoading(std::string{}, false);
        selectedServerKey.clear();
        return;
    }

    const std::string selectedKey = makeServerDetailsKey(*selectedEntry);
    selectedServerKey = selectedKey;

    if (selectedKey.empty()) {
        browser.setServerDescriptionLoading(std::string{}, false);
        return;
    }

    if (serverDescriptionCache.find(selectedKey) != serverDescriptionCache.end()) {
        browser.setServerDescriptionLoading(selectedKey, false);
        browser.setServerDescriptionError(selectedKey, std::string{});
        return;
    }
    if (serverDescriptionFailed.find(selectedKey) != serverDescriptionFailed.end()) {
        browser.setServerDescriptionLoading(selectedKey, false);
        auto errIt = serverDescriptionErrors.find(selectedKey);
        browser.setServerDescriptionError(selectedKey, errIt != serverDescriptionErrors.end() ? errIt->second : std::string{});
        return;
    }

    bool requestInFlight = serverDetailsRequest && !serverDetailsRequest->done.load();
    if (!requestInFlight) {
        startServerDetailsRequest(*selectedEntry);
    }
    browser.setServerDescriptionLoading(selectedKey, true);
    browser.setServerDescriptionError(selectedKey, std::string{});
}

void CommunityBrowserController::handleDisconnected(const std::string &reason) {
    std::string status = reason.empty()
        ? std::string("Disconnected from server. Select a server to reconnect.")
        : reason;

    browser.show(lastGuiEntries);
    browser.setConnectionState({});
    browser.setStatus(status, true);
    triggerFullRefresh();
}

void CommunityBrowserController::refreshGuiServerListOptions() {
    std::vector<ui::ServerListOption> options;

    if (clientConfig.showLanServers) {
        ui::ServerListOption lanOption;
        lanOption.name = "Local Area Network";
        options.push_back(std::move(lanOption));
    }

    for (const auto &source : clientConfig.serverLists) {
        ui::ServerListOption option;
        option.name = resolveDisplayNameForSource(source);
        option.host = source.host;
        options.push_back(std::move(option));
    }

    int optionCount = static_cast<int>(options.size());
    if (optionCount == 0) {
        activeServerListIndex = -1;
    } else if (activeServerListIndex < 0 || activeServerListIndex >= optionCount) {
        int desiredIndex = computeDefaultSelectionIndex(optionCount);
        if (desiredIndex < 0 || desiredIndex >= optionCount) {
            desiredIndex = 0;
        }
        activeServerListIndex = desiredIndex;
    }

    browser.setListOptions(options, activeServerListIndex);
}

std::vector<ClientServerListSource> CommunityBrowserController::resolveActiveServerLists() const {
    std::vector<ClientServerListSource> result;
    if (const auto *source = getSelectedRemoteSource()) {
        result.push_back(*source);
    }
    return result;
}

void CommunityBrowserController::rebuildServerListFetcher() {
    auto sources = resolveActiveServerLists();
    if (sources.empty()) {
        serverListFetcher.reset();
        cachedRemoteServers.clear();
        cachedSourceStatuses.clear();
        lastServerListGeneration = 0;
        return;
    }

    serverListFetcher = std::make_unique<ServerListFetcher>(std::move(sources));
    cachedRemoteServers.clear();
    cachedSourceStatuses.clear();
    lastServerListGeneration = 0;
    serverListFetcher->requestRefresh();
}

void CommunityBrowserController::handleServerListSelection(int selectedIndex) {
    int optionCount = totalListOptionCount();
    if (optionCount == 0) {
        return;
    }

    if (selectedIndex < 0) {
        selectedIndex = 0;
    } else if (selectedIndex >= optionCount) {
        selectedIndex = optionCount - 1;
    }

    if (selectedIndex == activeServerListIndex) {
        return;
    }

    activeServerListIndex = selectedIndex;
    rebuildServerListFetcher();
    rebuildEntries();

    if (isLanSelected()) {
        browser.setStatus("Local Area Network selected.", false);
    } else {
        browser.setStatus("Server list updated.", false);
    }

    triggerFullRefresh();
}

void CommunityBrowserController::handleServerListAddition(const ui::ServerListOption &option) {
    std::string trimmedHost = trimCopy(option.host);

    if (trimmedHost.empty()) {
        browser.showErrorDialog("Enter a community host before saving.");
        return;
    }
    ParsedCommunityHost parsed;
    if (!parseCommunityUrl(trimmedHost, parsed)) {
        browser.showErrorDialog(trimmedHost + " is not a valid Community site.");
        return;
    }
    if (pendingAddRequest && !pendingAddRequest->done.load()) {
        browser.showErrorDialog("Already checking a community. Please wait.");
        return;
    }

    auto existing = std::find_if(clientConfig.serverLists.begin(), clientConfig.serverLists.end(),
        [&](const ClientServerListSource &source) {
            return source.host == parsed.baseUrl;
        });
    if (existing != clientConfig.serverLists.end()) {
        std::string displayName = parsed.displayHost;
        if (!existing->name.empty()) {
            displayName = existing->name;
        } else {
            auto nameIt = serverListDisplayNames.find(parsed.baseUrl);
            if (nameIt != serverListDisplayNames.end() && !nameIt->second.empty()) {
                displayName = nameIt->second;
            }
        }
        browser.showErrorDialog("You already added \"" + displayName + "\".");
        return;
    }

    auto request = std::make_unique<PendingAddRequest>();
    request->baseUrl = parsed.baseUrl;
    request->displayHost = parsed.displayHost;
    request->worker = std::thread([req = request.get()]() {
        req->ok = fetchCommunityInfoOk(req->baseUrl);
        req->done.store(true);
    });
    pendingAddRequest = std::move(request);
    browser.setListStatus("Checking community...", false);
}

void CommunityBrowserController::commitServerListAddition(const std::string &baseUrl) {
    std::string trimmedHost = trimCopy(baseUrl);

    ClientServerListSource source;
    source.host = trimmedHost;
    clientConfig.serverLists.push_back(source);

    if (!clientConfig.Save(clientConfigPath)) {
        clientConfig.serverLists.pop_back();
        browser.setListStatus("Failed to write " + clientConfigPath + ". Check permissions.", true);
        return;
    }

    browser.setListStatus("Server list saved.", false);
    browser.clearNewListInputs();

    activeServerListIndex = getLanOffset() + static_cast<int>(clientConfig.serverLists.size()) - 1;
    refreshGuiServerListOptions();
    rebuildServerListFetcher();
    triggerFullRefresh();
}

void CommunityBrowserController::handleServerListDeletion(const std::string &host) {
    std::string trimmedHost = trimCopy(host);
    if (trimmedHost.empty()) {
        browser.setStatus("Select a community to delete.", true);
        return;
    }

    auto it = std::find_if(clientConfig.serverLists.begin(), clientConfig.serverLists.end(),
        [&](const ClientServerListSource &source) {
            return source.host == trimmedHost;
        });
    if (it == clientConfig.serverLists.end()) {
        browser.setStatus("Community not found.", true);
        return;
    }

    const int lanOffset = getLanOffset();
    const int removedIndex = static_cast<int>(std::distance(clientConfig.serverLists.begin(), it));
    const int removedOptionIndex = lanOffset + removedIndex;

    ClientServerListSource removedSource = *it;
    clientConfig.serverLists.erase(it);

    std::string previousDefault = clientConfig.defaultServerList;
    if (clientConfig.defaultServerList == trimmedHost) {
        clientConfig.defaultServerList.clear();
    }

    if (!clientConfig.Save(clientConfigPath)) {
        clientConfig.serverLists.insert(clientConfig.serverLists.begin() + removedIndex, removedSource);
        clientConfig.defaultServerList = previousDefault;
        browser.setStatus("Failed to update " + clientConfigPath + ". Check permissions.", true);
        return;
    }

    serverListDisplayNames.erase(trimmedHost);

    int optionCount = totalListOptionCount();
    if (activeServerListIndex > removedOptionIndex) {
        activeServerListIndex -= 1;
    } else if (activeServerListIndex == removedOptionIndex) {
        if (optionCount <= 0) {
            activeServerListIndex = -1;
        } else {
            activeServerListIndex = std::min(activeServerListIndex, optionCount - 1);
        }
    }

    browser.setStatus("Community removed.", false);
    refreshGuiServerListOptions();
    rebuildServerListFetcher();
    rebuildEntries();
    triggerFullRefresh();
}

void CommunityBrowserController::handleJoinSelection(const ui::CommunityBrowserSelection &selection) {
    std::string username = trimCopy(browser.getUsername());
    if (username.empty()) {
        browser.setStatus("Enter a username before joining.", true);
        return;
    }

    std::string password = browser.getPassword();
    const std::string storedHash = browser.getStoredPasswordHash();
    std::string communityHost = resolveCommunityHost(selection);

    pendingJoin.reset();

    if (communityHost.empty()) {
        connector.connect(selection.host, selection.port, username, false, false, false);
        return;
    }

    if (password.empty() && !storedHash.empty()) {
        spdlog::info("Authenticating '{}' on community {} (stored hash)", username, communityHost);
        browser.setStatus("Authenticating...", false);
        browser.storeCommunityAuth(communityHost, username, storedHash, std::string{});
        pendingJoin = PendingJoin{selection, communityHost, username, std::string{}, false, false, true};
        authClient.requestAuth(communityHost, username, storedHash, selection.worldName);
        return;
    }

    if (password.empty()) {
        spdlog::info("Checking username '{}' on community {}", username, communityHost);
        browser.setStatus("Checking username availability...", false);
        pendingJoin = PendingJoin{selection, communityHost, username, std::string{}, false, false, false};
        authClient.requestUserRegistered(communityHost, username);
        return;
    }

    std::string cacheKey = makeAuthCacheKey(communityHost, username);
    auto saltIt = passwordSaltCache.find(cacheKey);
    if (saltIt == passwordSaltCache.end()) {
        spdlog::info("Fetching auth salt for '{}' on community {}", username, communityHost);
        browser.setStatus("Fetching account info...", false);
        pendingJoin = PendingJoin{selection, communityHost, username, password, false, false, false};
        authClient.requestUserRegistered(communityHost, username);
        return;
    }

    std::string passhash;
    if (!client::auth::HashPasswordPBKDF2Sha256(password, saltIt->second, passhash)) {
        browser.setStatus("Failed to hash password.", true);
        return;
    }

    spdlog::info("Authenticating '{}' on community {}", username, communityHost);
    browser.setStatus("Authenticating...", false);
    browser.storeCommunityAuth(communityHost, username, passhash, saltIt->second);
    pendingJoin = PendingJoin{selection, communityHost, username, std::string{}, false, false, true};
    authClient.requestAuth(communityHost, username, passhash, selection.worldName);
}

void CommunityBrowserController::handleAuthResponse(const CommunityAuthClient::Response &response) {
    if (!pendingJoin) {
        return;
    }

    const auto &pending = *pendingJoin;
    if (pending.communityHost != response.host || pending.username != response.username) {
        return;
    }

    if (response.type == CommunityAuthClient::RequestType::UserRegistered) {
        if (!response.ok) {
            spdlog::warn("Community auth: user_registered failed for '{}' on {}: {}",
                         response.username,
                         response.host,
                         response.error.empty() ? "unknown_error" : response.error);
            browser.setStatus("Failed to reach community server.", true);
            pendingJoin.reset();
            return;
        }

        std::string cacheKey = makeAuthCacheKey(response.host, response.username);
        if (!response.salt.empty()) {
            passwordSaltCache[cacheKey] = response.salt;
        }

        if (response.registered && (response.locked || response.deleted)) {
            if (response.locked) {
                browser.setStatus("This username is locked out. Please contact an admin.", true);
            } else {
                browser.setStatus("That username is unavailable on this community.", true);
            }
            pendingJoin.reset();
            return;
        }

        if (pending.password.empty()) {
            if (response.registered) {
                std::string communityLabel = response.communityName.empty()
                    ? response.host
                    : response.communityName;
                browser.setStatus(
                    "Username is registered on " + communityLabel + ". Enter your password to join.",
                    true);
                pendingJoin.reset();
            } else {
                pendingJoin.reset();
                spdlog::info("Connecting as anonymous user '{}' to {}:{}",
                             pending.username,
                             pending.selection.host,
                             pending.selection.port);
                connector.connect(pending.selection.host, pending.selection.port, pending.username, false, false, false);
            }
            return;
        }

        if (!response.registered) {
            pendingJoin.reset();
            spdlog::info("Connecting as anonymous user '{}' to {}:{}",
                         pending.username,
                         pending.selection.host,
                         pending.selection.port);
            connector.connect(pending.selection.host, pending.selection.port, pending.username, false, false, false);
            return;
        }

        if (response.salt.empty()) {
            browser.setStatus("Missing password salt from community.", true);
            pendingJoin.reset();
            return;
        }

        std::string passhash;
        if (!client::auth::HashPasswordPBKDF2Sha256(pending.password, response.salt, passhash)) {
            browser.setStatus("Failed to hash password.", true);
            pendingJoin.reset();
            return;
        }

        spdlog::info("Authenticating '{}' on community {}", response.username, response.host);
        browser.setStatus("Authenticating...", false);
        browser.storeCommunityAuth(response.host, response.username, passhash, response.salt);
        pendingJoin->password.clear();
        pendingJoin->awaitingAuth = true;
        authClient.requestAuth(response.host, response.username, passhash, pending.selection.worldName);
        return;
    }

    if (!response.ok) {
        spdlog::warn("Community auth: authentication failed for '{}' on {}: {}",
                     response.username,
                     response.host,
                     response.error.empty() ? "unknown_error" : response.error);
        browser.setStatus("Authentication failed.", true);
        pendingJoin.reset();
        return;
    }

    spdlog::info("Connecting as registered user '{}' to {}:{}",
                 pending.username,
                 pending.selection.host,
                 pending.selection.port);
    browser.clearPassword();
    connector.connect(
        pending.selection.host,
        pending.selection.port,
        pending.username,
        true,
        response.communityAdmin,
        response.localAdmin);
    pendingJoin.reset();
}

std::string CommunityBrowserController::resolveCommunityHost(const ui::CommunityBrowserSelection &selection) const {
    if (!selection.sourceHost.empty()) {
        return selection.sourceHost;
    }
    if (!selection.fromPreset) {
        if (const auto *source = getSelectedRemoteSource()) {
            return source->host;
        }
    }
    return {};
}

std::string CommunityBrowserController::makeAuthCacheKey(const std::string &host, const std::string &username) const {
    return host + "\n" + username;
}

void CommunityBrowserController::updateServerListDisplayNamesFromCache() {
    bool displayNamesChanged = false;
    bool configUpdated = false;
    std::vector<std::pair<std::size_t, std::string>> previousNames;

    for (const auto &record : cachedRemoteServers) {
        if (record.sourceHost.empty() || record.sourceName.empty()) {
            continue;
        }

        auto mapIt = serverListDisplayNames.find(record.sourceHost);
        if (mapIt == serverListDisplayNames.end() || mapIt->second != record.sourceName) {
            serverListDisplayNames[record.sourceHost] = record.sourceName;
            displayNamesChanged = true;
        }

        for (std::size_t i = 0; i < clientConfig.serverLists.size(); ++i) {
            auto &source = clientConfig.serverLists[i];
            if (source.host != record.sourceHost) {
                continue;
            }

            if (source.name != record.sourceName) {
                previousNames.emplace_back(i, source.name);
                source.name = record.sourceName;
                configUpdated = true;
            }
            break;
        }
    }

    for (const auto &status : cachedSourceStatuses) {
        if (status.sourceHost.empty() || status.communityName.empty()) {
            continue;
        }

        auto mapIt = serverListDisplayNames.find(status.sourceHost);
        if (mapIt == serverListDisplayNames.end() || mapIt->second != status.communityName) {
            serverListDisplayNames[status.sourceHost] = status.communityName;
            displayNamesChanged = true;
        }

        for (std::size_t i = 0; i < clientConfig.serverLists.size(); ++i) {
            auto &source = clientConfig.serverLists[i];
            if (source.host != status.sourceHost) {
                continue;
            }

            if (source.name != status.communityName) {
                previousNames.emplace_back(i, source.name);
                source.name = status.communityName;
                configUpdated = true;
            }
            break;
        }
    }

    if (configUpdated) {
        if (!clientConfig.Save(clientConfigPath)) {
            for (const auto &entry : previousNames) {
                clientConfig.serverLists[entry.first].name = entry.second;
            }
            spdlog::warn("CommunityBrowserController: Failed to persist server list names to {}.", clientConfigPath);
        } else {
            displayNamesChanged = true;
        }
    }

    if (displayNamesChanged) {
        refreshGuiServerListOptions();
    }
}

void CommunityBrowserController::updateCommunityDetails() {
    if (isLanSelected()) {
        browser.setCommunityDetails(std::string{});
        return;
    }

    const auto *source = getSelectedRemoteSource();
    if (!source) {
        browser.setCommunityDetails(std::string{});
        return;
    }

    std::string details;
    for (const auto &status : cachedSourceStatuses) {
        if (status.sourceHost == source->host) {
            details = status.communityDetails;
            break;
        }
    }

    browser.setCommunityDetails(details);
}

std::string CommunityBrowserController::makeServerDetailsKey(const ui::CommunityBrowserEntry &entry) const {
    if (entry.sourceHost.empty()) {
        return {};
    }
    if (entry.code.empty()) {
        return {};
    }
    return entry.sourceHost + "|" + entry.code;
}

std::string CommunityBrowserController::makeServerDetailsKey(const ServerListFetcher::ServerRecord &record) const {
    if (record.sourceHost.empty()) {
        return {};
    }
    if (record.code.empty()) {
        return {};
    }
    return record.sourceHost + "|" + record.code;
}

void CommunityBrowserController::startServerDetailsRequest(const ui::CommunityBrowserEntry &entry) {
    if (!curlReady) {
        return;
    }
    if (entry.sourceHost.empty() || entry.code.empty()) {
        return;
    }

    auto request = std::make_unique<ServerDetailsRequest>();
    request->key = makeServerDetailsKey(entry);
    request->sourceHost = entry.sourceHost;
    request->serverName = entry.code;

    request->worker = std::thread([req = request.get()]() {
        std::string url = buildServerDetailsUrl(req->sourceHost, req->serverName);
        if (url.empty()) {
            req->ok = false;
            req->description = "Missing server details URL.";
            req->done.store(true);
            return;
        }

        std::string body;
        long status = 0;
        if (!fetchUrl(url, body, &status)) {
            req->ok = false;
            if (status != 0) {
                req->description = "Server details request failed (HTTP " + std::to_string(status) + ").";
            } else {
                req->description = "Server details request failed.";
            }
            req->done.store(true);
            return;
        }

        try {
            bz::json::Value jsonData = bz::json::Parse(body);
            const auto *serverNode = jsonData.contains("server") ? &jsonData["server"] : nullptr;
            if (serverNode && serverNode->is_object()) {
                if (auto nameIt = serverNode->find("name");
                    nameIt != serverNode->end() && nameIt->is_string()) {
                    req->detailName = nameIt->get<std::string>();
                }
                if (auto descIt = serverNode->find("description");
                    descIt != serverNode->end() && descIt->is_string()) {
                    req->description = descIt->get<std::string>();
                } else if (auto descIt2 = serverNode->find("overview");
                           descIt2 != serverNode->end() && descIt2->is_string()) {
                    req->description = descIt2->get<std::string>();
                }
            } else if (auto descIt = jsonData.find("description");
                       descIt != jsonData.end() && descIt->is_string()) {
                req->description = descIt->get<std::string>();
            } else if (auto descIt2 = jsonData.find("overview");
                       descIt2 != jsonData.end() && descIt2->is_string()) {
                req->description = descIt2->get<std::string>();
            }
            req->ok = !req->description.empty();
        } catch (const std::exception &ex) {
            spdlog::warn("CommunityBrowserController: Failed to parse server details: {}", ex.what());
            req->ok = false;
            req->description = "Failed to parse server details.";
        }
        req->done.store(true);
    });

    serverDetailsRequest = std::move(request);
}

std::string CommunityBrowserController::resolveDisplayNameForSource(const ClientServerListSource &source) const {
    auto it = serverListDisplayNames.find(source.host);
    if (it != serverListDisplayNames.end() && !it->second.empty()) {
        return it->second;
    }

    if (!source.name.empty()) {
        return source.name;
    }

    return source.host;
}

int CommunityBrowserController::getLanOffset() const {
    return clientConfig.showLanServers ? 1 : 0;
}

int CommunityBrowserController::totalListOptionCount() const {
    return getLanOffset() + static_cast<int>(clientConfig.serverLists.size());
}

bool CommunityBrowserController::isLanIndex(int index) const {
    return clientConfig.showLanServers && index == 0;
}

bool CommunityBrowserController::isLanSelected() const {
    return isLanIndex(activeServerListIndex);
}

const ClientServerListSource* CommunityBrowserController::getSelectedRemoteSource() const {
    if (activeServerListIndex < 0) {
        return nullptr;
    }

    int lanOffset = getLanOffset();
    if (activeServerListIndex < lanOffset) {
        return nullptr;
    }

    int remoteIndex = activeServerListIndex - lanOffset;
    if (remoteIndex < 0 || remoteIndex >= static_cast<int>(clientConfig.serverLists.size())) {
        return nullptr;
    }

    return &clientConfig.serverLists[remoteIndex];
}

int CommunityBrowserController::computeDefaultSelectionIndex(int optionCount) const {
    if (optionCount == 0) {
        return -1;
    }

    std::string trimmedDefault = trimCopy(clientConfig.defaultServerList);
    if (clientConfig.showLanServers) {
        if (trimmedDefault.empty() || isLanToken(trimmedDefault)) {
            return 0;
        }
    }

    if (!trimmedDefault.empty()) {
        for (std::size_t i = 0; i < clientConfig.serverLists.size(); ++i) {
            if (clientConfig.serverLists[i].host == trimmedDefault) {
                return getLanOffset() + static_cast<int>(i);
            }
        }
    }

    if (clientConfig.showLanServers) {
        return 0;
    }

    return 0;
}
