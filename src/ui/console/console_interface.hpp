#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "ui/console/console_types.hpp"

namespace ui {

class ConsoleInterface {
public:
    using MessageTone = ui::MessageTone;
    struct ConnectionState {
        bool connected = false;
        std::string host;
        uint16_t port = 0;
    };

    virtual ~ConsoleInterface() = default;

    virtual void show(const std::vector<CommunityBrowserEntry> &entries) = 0;
    virtual void setEntries(const std::vector<CommunityBrowserEntry> &entries) = 0;
    virtual void setListOptions(const std::vector<ServerListOption> &options, int selectedIndex) = 0;
    virtual void hide() = 0;
    virtual bool isVisible() const = 0;
    virtual void setStatus(const std::string &statusText, bool isErrorMessage) = 0;
    virtual void setCommunityDetails(const std::string &detailsText) = 0;
    virtual void setServerDescriptionLoading(const std::string &key, bool loading) = 0;
    virtual bool isServerDescriptionLoading(const std::string &key) const = 0;
    virtual void setServerDescriptionError(const std::string &key, const std::string &message) = 0;
    virtual std::optional<std::string> getServerDescriptionError(const std::string &key) const = 0;
    virtual std::optional<CommunityBrowserSelection> consumeSelection() = 0;
    virtual std::optional<int> consumeListSelection() = 0;
    virtual std::optional<ServerListOption> consumeNewListRequest() = 0;
    virtual std::optional<std::string> consumeDeleteListRequest() = 0;
    virtual void setListStatus(const std::string &statusText, bool isErrorMessage) = 0;
    virtual void clearNewListInputs() = 0;
    virtual std::string getUsername() const = 0;
    virtual std::string getPassword() const = 0;
    virtual std::string getStoredPasswordHash() const = 0;
    virtual void clearPassword() = 0;
    virtual void storeCommunityAuth(const std::string &communityHost,
                                    const std::string &username,
                                    const std::string &passhash,
                                    const std::string &salt) = 0;
    virtual void setCommunityStatus(const std::string &text, MessageTone tone) = 0;
    virtual std::optional<CommunityBrowserEntry> getSelectedEntry() const = 0;
    virtual bool consumeRefreshRequest() = 0;
    virtual void setScanning(bool scanning) = 0;
    virtual void setUserConfigPath(const std::string &path) = 0;
    virtual bool consumeFontReloadRequest() = 0;
    virtual void setConnectionState(const ConnectionState &state) = 0;
    virtual ConnectionState getConnectionState() const = 0;
    virtual bool consumeQuitRequest() = 0;
    virtual void showErrorDialog(const std::string &message) = 0;
};

} // namespace ui
