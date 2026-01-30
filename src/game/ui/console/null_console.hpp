#pragma once

#include <optional>
#include <string>
#include <type_traits>
#include <vector>

#include "ui/console/console_interface.hpp"
#include "ui/console/console_types.hpp"

namespace ui {

class NullConsole final : public ConsoleInterface {
public:
    void show(const std::vector<CommunityBrowserEntry> &entriesIn) override {
        entries = entriesIn;
        visible = true;
    }

    void setEntries(const std::vector<CommunityBrowserEntry> &entriesIn) override {
        entries = entriesIn;
    }

    void setListOptions(const std::vector<ServerListOption> &options, int selectedIndexIn) override {
        listOptions = options;
        listSelectedIndex = selectedIndexIn;
    }

    void hide() override {
        visible = false;
    }

    bool isVisible() const override {
        return visible;
    }

    void setStatus(const std::string &statusTextIn, bool isErrorMessageIn) override {
        statusText = statusTextIn;
        statusIsError = isErrorMessageIn;
    }

    void setCommunityDetails(const std::string &detailsTextIn) override {
        communityDetailsText = detailsTextIn;
    }

    void setServerDescriptionLoading(const std::string &key, bool loading) override {
        serverDescriptionLoadingKey = key;
        serverDescriptionLoading = loading;
    }

    bool isServerDescriptionLoading(const std::string &key) const override {
        return serverDescriptionLoading && key == serverDescriptionLoadingKey;
    }

    void setServerDescriptionError(const std::string &key, const std::string &message) override {
        serverDescriptionErrorKey = key;
        serverDescriptionErrorText = message;
    }

    std::optional<std::string> getServerDescriptionError(const std::string &key) const override {
        if (key.empty() || key != serverDescriptionErrorKey) {
            return std::nullopt;
        }
        return serverDescriptionErrorText;
    }

    std::optional<CommunityBrowserSelection> consumeSelection() override {
        return consumeOptional(pendingSelection);
    }

    std::optional<int> consumeListSelection() override {
        return consumeOptional(pendingListSelection);
    }

    std::optional<ServerListOption> consumeNewListRequest() override {
        return consumeOptional(pendingNewList);
    }

    std::optional<std::string> consumeDeleteListRequest() override {
        return consumeOptional(pendingDeleteListHost);
    }

    void setListStatus(const std::string &statusTextIn, bool isErrorMessageIn) override {
        listStatusText = statusTextIn;
        listStatusIsError = isErrorMessageIn;
    }

    void clearNewListInputs() override {
        newListHost.clear();
    }

    std::string getUsername() const override {
        return username;
    }

    std::string getPassword() const override {
        return password;
    }

    std::string getStoredPasswordHash() const override {
        return storedPasswordHash;
    }

    void clearPassword() override {
        password.clear();
    }

    void storeCommunityAuth(const std::string &communityHost,
                            const std::string &usernameIn,
                            const std::string &passhash,
                            const std::string &salt) override {
        (void)communityHost;
        (void)salt;
        username = usernameIn;
        storedPasswordHash = passhash;
    }

    void setCommunityStatus(const std::string &text, MessageTone tone) override {
        statusText = text;
        statusIsError = (tone == MessageTone::Error);
    }

    std::optional<CommunityBrowserEntry> getSelectedEntry() const override {
        if (selectedIndex < 0 || selectedIndex >= static_cast<int>(entries.size())) {
            return std::nullopt;
        }
        return entries[static_cast<std::size_t>(selectedIndex)];
    }

    bool consumeRefreshRequest() override {
        if (!refreshRequested) {
            return false;
        }
        refreshRequested = false;
        return true;
    }

    void setScanning(bool scanningIn) override {
        scanning = scanningIn;
    }

    void setUserConfigPath(const std::string &path) override {
        userConfigPath = path;
    }

    void setConnectionState(const ConnectionState &state) override {
        connectionState = state;
    }

    ConnectionState getConnectionState() const override {
        return connectionState;
    }

    bool consumeQuitRequest() override {
        if (!pendingQuitRequest) {
            return false;
        }
        pendingQuitRequest = false;
        return true;
    }

    bool consumeFontReloadRequest() override {
        return false;
    }

    bool consumeKeybindingsReloadRequest() override {
        return false;
    }

    void showErrorDialog(const std::string &message) override {
        errorDialogMessage = message;
    }

private:
    template <typename T>
    static std::optional<T> consumeOptional(std::optional<T> &value) {
        if (!value) {
            return std::nullopt;
        }
        std::optional<T> out = std::move(value);
        value.reset();
        return out;
    }

    std::vector<CommunityBrowserEntry> entries;
    std::vector<ServerListOption> listOptions;
    int listSelectedIndex = -1;
    bool visible = false;
    std::string statusText;
    bool statusIsError = false;
    std::string listStatusText;
    bool listStatusIsError = false;
    std::string communityDetailsText;
    std::string serverDescriptionLoadingKey;
    bool serverDescriptionLoading = false;
    std::string serverDescriptionErrorKey;
    std::string serverDescriptionErrorText;
    std::optional<CommunityBrowserSelection> pendingSelection;
    std::optional<int> pendingListSelection;
    std::optional<ServerListOption> pendingNewList;
    std::optional<std::string> pendingDeleteListHost;
    std::string newListHost;
    std::string username;
    std::string password;
    std::string storedPasswordHash;
    int selectedIndex = -1;
    bool refreshRequested = false;
    bool scanning = false;
    std::string userConfigPath;
    ConnectionState connectionState{};
    bool pendingQuitRequest = false;
    std::string errorDialogMessage;
};

} // namespace ui
