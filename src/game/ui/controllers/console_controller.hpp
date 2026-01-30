#pragma once

#include <optional>
#include <string>

#include "ui/models/console_model.hpp"

namespace ui {

class ConsoleController {
public:
    struct Credentials {
        std::string username;
        std::string storedPasswordHash;
    };

    struct PersistResult {
        bool clearStoredPasswordHash = false;
    };

    explicit ConsoleController(ConsoleModel &model);

    std::string communityKeyForIndex(int index) const;
    Credentials loadCommunityCredentials(int listIndex) const;
    PersistResult persistCommunityCredentials(int listIndex,
                                              const std::string &username,
                                              const std::string &storedPasswordHash,
                                              bool passwordChanged) const;
    void queueSelection(const CommunityBrowserSelection &selection);
    void queueListSelection(int index);
    void queueNewListRequest(const ServerListOption &option);
    void queueDeleteListRequest(const std::string &host);
    void requestRefresh();
    void clearPending();

    std::optional<CommunityBrowserSelection> consumeSelection();
    std::optional<int> consumeListSelection();
    std::optional<ServerListOption> consumeNewListRequest();
    std::optional<std::string> consumeDeleteListRequest();
    bool consumeRefreshRequest();

private:
    ConsoleModel &model;
    std::optional<CommunityBrowserSelection> pendingSelection;
    std::optional<int> pendingListSelection;
    std::optional<ServerListOption> pendingNewList;
    std::optional<std::string> pendingDeleteListHost;
    bool refreshRequested = false;
};

} // namespace ui
