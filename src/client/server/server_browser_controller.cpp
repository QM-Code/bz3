#include "client/server/server_browser_controller.hpp"

#include <algorithm>
#include <cctype>
#include <unordered_map>
#include <unordered_set>
#include <utility>

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

ServerBrowserController::ServerBrowserController(ClientEngine &engine,
                                                 ClientConfig &clientConfig,
                                                 const std::string &configPath,
                                                 const std::string &defaultHost,
                                                 uint16_t defaultPort,
                                                 ServerConnector &connector)
    : engine(engine),
            browser(engine.gui->serverBrowser()),
      clientConfig(clientConfig),
      clientConfigPath(configPath),
            connector(connector),
            defaultHost(defaultHost.empty() ? "localhost" : defaultHost),
            defaultPort(applyPortFallback(defaultPort)) {
    auto toInterval = [](int seconds) {
        return seconds > 0 ? std::chrono::seconds(seconds) : std::chrono::seconds{0};
    };

    communityAutoRefreshInterval = toInterval(clientConfig.communityAutoRefreshSeconds);
    lanAutoRefreshInterval = toInterval(clientConfig.lanAutoRefreshSeconds);

    refreshGuiServerListOptions();
    rebuildServerListFetcher();

                browser.show({}, this->defaultHost, this->defaultPort);
    triggerFullRefresh();
    if (lanAutoRefreshInterval.count() > 0) {
        nextAutoScanTime = SteadyClock::now() + lanAutoRefreshInterval;
    } else {
        nextAutoScanTime = SteadyClock::time_point{};
    }
}

void ServerBrowserController::triggerFullRefresh() {
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
        if (communityAutoRefreshInterval.count() > 0) {
            nextRemoteRefreshTime = nowSteady + communityAutoRefreshInterval;
        } else {
            nextRemoteRefreshTime = SteadyClock::time_point{};
        }
    } else {
        nextRemoteRefreshTime = SteadyClock::time_point{};
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
        browser.setStatus("Searching local network and fetching the selected server list...", false);
    } else if (lanActive) {
        browser.setStatus("Searching local network for servers...", false);
    } else {
        browser.setStatus("Fetching " + selectionLabel + "...", false);
    }

    browser.setScanning(issuedRequest);
}

void ServerBrowserController::rebuildEntries() {
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

    std::vector<gui::ServerBrowserEntry> entries;
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
            gui::ServerBrowserEntry entry;
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
        gui::ServerBrowserEntry entry;
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
        entries.push_back(std::move(entry));
    }

    lastGuiEntries = entries;
    browser.setEntries(lastGuiEntries);
    if (!entries.empty()) {
        browser.setStatus("Select a server to connect.", false);
    }
}

void ServerBrowserController::update() {
    if (auto listSelection = browser.consumeListSelection()) {
        handleServerListSelection(*listSelection);
    }

    if (auto newList = browser.consumeNewListRequest()) {
        handleServerListAddition(*newList);
    }

    auto nowSteady = SteadyClock::now();
    const bool remoteListActive = static_cast<bool>(serverListFetcher);

    if (browser.consumeRefreshRequest()) {
        triggerFullRefresh();
        if (lanAutoRefreshInterval.count() > 0) {
            nextAutoScanTime = nowSteady + lanAutoRefreshInterval;
        } else {
            nextAutoScanTime = SteadyClock::time_point{};
        }
    } else if (lanAutoRefreshInterval.count() > 0 && !discovery.isScanning() && nowSteady >= nextAutoScanTime) {
        triggerFullRefresh();
        nextAutoScanTime = nowSteady + lanAutoRefreshInterval;
    }

    if (!remoteListActive) {
        nextRemoteRefreshTime = SteadyClock::time_point{};
    } else {
        if (communityAutoRefreshInterval.count() == 0) {
            nextRemoteRefreshTime = SteadyClock::time_point{};
        } else if (nextRemoteRefreshTime == SteadyClock::time_point{}) {
            nextRemoteRefreshTime = nowSteady + communityAutoRefreshInterval;
        }

        if (communityAutoRefreshInterval.count() > 0 && !serverListFetcher->isFetching() && nowSteady >= nextRemoteRefreshTime) {
            serverListFetcher->requestRefresh();
            nextRemoteRefreshTime = nowSteady + communityAutoRefreshInterval;
        }
    }

    discovery.update();
    bool remoteFetchingActive = remoteListActive && serverListFetcher->isFetching();
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
            lastServerListGeneration = remoteGeneration;
            entriesDirty = true;
            updateServerListDisplayNamesFromCache();
        }
    }

    if (entriesDirty) {
        rebuildEntries();
    }

    const auto &servers = discovery.getServers();
    bool lanEmpty = servers.empty();
    bool remoteEmpty = cachedRemoteServers.empty();

    if (auto selection = browser.consumeSelection()) {
        connector.connect(selection->host, selection->port);
    }

    const ClientServerListSource *selectedRemoteSource = getSelectedRemoteSource();
    std::string remoteLabel = selectedRemoteSource ? resolveDisplayNameForSource(*selectedRemoteSource) : "selected server list";

    if (lanEmpty && remoteEmpty) {
        if (discovery.isScanning() && isLanSelected()) {
            browser.setStatus("Searching local network for servers...", false);
        } else if (remoteFetchingActive && serverListFetcher) {
            browser.setStatus("Fetching " + remoteLabel + "...", false);
        } else if (isLanSelected()) {
            browser.setStatus("No LAN servers found. Start one locally or refresh.", true);
        } else if (serverListFetcher) {
            browser.setStatus(remoteLabel + " returned no servers. Verify the list provider.", true);
        } else {
            browser.setStatus("No server sources configured. Add a server list or enable Local Area Network.", true);
        }
    }
}

void ServerBrowserController::handleDisconnected(const std::string &reason) {
    std::string status = reason.empty()
        ? std::string("Disconnected from server. Select a server to reconnect.")
        : reason;

    browser.show(lastGuiEntries, defaultHost, defaultPort);
    browser.setStatus(status, true);
    triggerFullRefresh();
    if (lanAutoRefreshInterval.count() > 0) {
        nextAutoScanTime = SteadyClock::now() + lanAutoRefreshInterval;
    } else {
        nextAutoScanTime = SteadyClock::time_point{};
    }
}

void ServerBrowserController::refreshGuiServerListOptions() {
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

std::vector<ClientServerListSource> ServerBrowserController::resolveActiveServerLists() const {
    std::vector<ClientServerListSource> result;
    if (const auto *source = getSelectedRemoteSource()) {
        result.push_back(*source);
    }
    return result;
}

void ServerBrowserController::rebuildServerListFetcher() {
    auto sources = resolveActiveServerLists();
    if (sources.empty()) {
        serverListFetcher.reset();
        cachedRemoteServers.clear();
        lastServerListGeneration = 0;
        nextRemoteRefreshTime = SteadyClock::time_point{};
        return;
    }

    serverListFetcher = std::make_unique<ServerListFetcher>(std::move(sources));
    cachedRemoteServers.clear();
    lastServerListGeneration = 0;
    serverListFetcher->requestRefresh();
    if (communityAutoRefreshInterval.count() > 0) {
        nextRemoteRefreshTime = SteadyClock::now() + communityAutoRefreshInterval;
    } else {
        nextRemoteRefreshTime = SteadyClock::time_point{};
    }
}

void ServerBrowserController::handleServerListSelection(int selectedIndex) {
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
        browser.setListStatus("Local Area Network selected.", false);
    } else {
        browser.setListStatus("Server list updated.", false);
    }

    triggerFullRefresh();
}

void ServerBrowserController::handleServerListAddition(const gui::ServerListOption &option) {
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

void ServerBrowserController::updateServerListDisplayNamesFromCache() {
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
            spdlog::warn("ServerBrowserController: Failed to persist server list names to {}.", clientConfigPath);
        } else {
            displayNamesChanged = true;
        }
    }

    if (displayNamesChanged) {
        refreshGuiServerListOptions();
    }
}

std::string ServerBrowserController::resolveDisplayNameForSource(const ClientServerListSource &source) const {
    auto it = serverListDisplayNames.find(source.host);
    if (it != serverListDisplayNames.end() && !it->second.empty()) {
        return it->second;
    }

    if (!source.name.empty()) {
        return source.name;
    }

    return source.host;
}

int ServerBrowserController::getLanOffset() const {
    return clientConfig.showLanServers ? 1 : 0;
}

int ServerBrowserController::totalListOptionCount() const {
    return getLanOffset() + static_cast<int>(clientConfig.serverLists.size());
}

bool ServerBrowserController::isLanIndex(int index) const {
    return clientConfig.showLanServers && index == 0;
}

bool ServerBrowserController::isLanSelected() const {
    return isLanIndex(activeServerListIndex);
}

const ClientServerListSource* ServerBrowserController::getSelectedRemoteSource() const {
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

int ServerBrowserController::computeDefaultSelectionIndex(int optionCount) const {
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
