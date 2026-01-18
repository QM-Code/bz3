#include "client/server/community_browser_controller.hpp"

#include <algorithm>
#include <cctype>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "client/server/password_hash.hpp"
#include "client/server/server_connector.hpp"
#include "common/data_path_resolver.hpp"
#include "spdlog/spdlog.h"

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
    if (const auto configured = bz::data::ConfigValueUInt16("network.ServerPort")) {
        return *configured;
    }
    return 0;
}

uint16_t applyPortFallback(uint16_t candidate) {
    if (candidate != 0) {
        return candidate;
    }
    return configuredServerPort();
}
}

CommunityBrowserController::CommunityBrowserController(ClientEngine &engine,
                                                       ClientConfig &clientConfig,
                                                       const std::string &configPath,
                                                       const std::string &defaultHost,
                                                       uint16_t defaultPort,
                                                       ServerConnector &connector)
    : engine(engine),
      browser(engine.gui->mainMenu()),
      clientConfig(clientConfig),
      clientConfigPath(configPath),
      connector(connector),
      defaultHost(defaultHost.empty() ? "localhost" : defaultHost),
      defaultPort(applyPortFallback(defaultPort)) {
    refreshGuiServerListOptions();
    rebuildServerListFetcher();

    browser.show({}, this->defaultHost, this->defaultPort);
    browser.setUserConfigPath(clientConfigPath);
    triggerFullRefresh();
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
                                   gui::MainMenuView::MessageTone::Pending);
    } else if (lanActive) {
        browser.setCommunityStatus("Searching local network for servers...",
                                   gui::MainMenuView::MessageTone::Pending);
    } else {
        browser.setCommunityStatus("Fetching " + selectionLabel + "...",
                                   gui::MainMenuView::MessageTone::Pending);
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

    std::vector<gui::CommunityBrowserEntry> entries;
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
            gui::CommunityBrowserEntry entry;
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
        gui::CommunityBrowserEntry entry;
        entry.label = record.name.empty() ? record.host : record.name;
        entry.host = record.host;
        entry.port = recordPort;
        entry.description = buildRemoteDescription(record);
        entry.displayHost = record.host;
        entry.longDescription = record.description.empty() ? entry.description : record.description;
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

    if (remoteFetchingActive && serverListFetcher && !isLanSelected()) {
        std::string selectionLabel = "selected server list";
        if (const auto *source = getSelectedRemoteSource()) {
            selectionLabel = resolveDisplayNameForSource(*source);
        }
        browser.setCommunityStatus("Fetching " + selectionLabel + "...",
                                   gui::MainMenuView::MessageTone::Pending);
    } else if (serverListFetcher && !isLanSelected()) {
        std::string statusText;
        gui::MainMenuView::MessageTone tone = gui::MainMenuView::MessageTone::Notice;
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
                    tone = gui::MainMenuView::MessageTone::Error;
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
                                   gui::MainMenuView::MessageTone::Pending);
    } else {
        browser.setCommunityStatus(std::string{}, gui::MainMenuView::MessageTone::Notice);
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
                                       gui::MainMenuView::MessageTone::Pending);
        } else if (remoteFetchingActive && serverListFetcher) {
            browser.setStatus(std::string{}, false);
        } else if (isLanSelected()) {
            browser.setStatus(std::string{}, false);
            browser.setCommunityStatus("No LAN servers found. Start one locally or refresh.",
                                       gui::MainMenuView::MessageTone::Notice);
        } else if (serverListFetcher) {
            browser.setStatus(std::string{}, false);
        } else {
            browser.setStatus("No server sources configured. Add a server list or enable Local Area Network.", true);
        }
    }
}

void CommunityBrowserController::handleDisconnected(const std::string &reason) {
    std::string status = reason.empty()
        ? std::string("Disconnected from server. Select a server to reconnect.")
        : reason;

    browser.show(lastGuiEntries, defaultHost, defaultPort);
    browser.setStatus(status, true);
    triggerFullRefresh();
}

void CommunityBrowserController::refreshGuiServerListOptions() {
    std::vector<gui::ServerListOption> options;

    if (clientConfig.showLanServers) {
        gui::ServerListOption lanOption;
        lanOption.name = "Local Area Network";
        options.push_back(std::move(lanOption));
    }

    for (const auto &source : clientConfig.serverLists) {
        gui::ServerListOption option;
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

void CommunityBrowserController::handleServerListAddition(const gui::ServerListOption &option) {
    std::string trimmedHost = trimCopy(option.host);

    if (trimmedHost.empty()) {
        browser.setListStatus("Enter a host before saving.", true);
        return;
    }

    auto existing = std::find_if(clientConfig.serverLists.begin(), clientConfig.serverLists.end(),
        [&](const ClientServerListSource &source) {
            return source.host == trimmedHost;
        });
    if (existing != clientConfig.serverLists.end()) {
        browser.setListStatus("A server list with that host already exists.", true);
        return;
    }

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

void CommunityBrowserController::handleJoinSelection(const gui::CommunityBrowserSelection &selection) {
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

std::string CommunityBrowserController::resolveCommunityHost(const gui::CommunityBrowserSelection &selection) const {
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
