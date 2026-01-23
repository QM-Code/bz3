#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <vector>
#include <nlohmann/json.hpp>

namespace Rml {
class Element;
class ElementDocument;
class EventListener;
}

#include "ui/console/console_interface.hpp"
#include "ui/console/console_types.hpp"
#include "ui/backends/rmlui/console/modal_dialog.hpp"
#include "ui/backends/rmlui/console/panels/panel.hpp"

namespace ui {

class RmlUiPanelCommunity final : public RmlUiPanel {
public:
    RmlUiPanelCommunity();

    void setListOptions(const std::vector<ServerListOption> &options, int selectedIndex);
    void setEntries(const std::vector<CommunityBrowserEntry> &entries);
    void setCommunityDetails(const std::string &details);
    void setAddStatus(const std::string &text, bool isError);
    void setServerDescriptionLoading(const std::string &key, bool loading);
    void setServerDescriptionError(const std::string &key, const std::string &message);
    void clearAddInput();
    std::string getUsernameValue() const;
    std::string getPasswordValue() const;
    std::string getStoredPasswordHashValue() const;
    void setStoredPasswordHashValue(const std::string &value);
    void clearPasswordValue();
    void setUsernameValue(const std::string &value);
    void setConnectionState(const ConsoleInterface::ConnectionState &state);
    void setPasswordHintActive(bool active);
    void persistCommunityCredentials(bool passwordChanged);
    void setUserConfigPath(const std::string &path);
    void refreshCommunityCredentials();
    void showErrorDialog(const std::string &message);
    std::optional<std::string> consumeDeleteListRequest();
    void bindCallbacks(std::function<void(int)> onSelection,
                       std::function<void(const std::string &)> onAdd,
                       std::function<void()> onRefresh,
                       std::function<void(int)> onServerSelection,
                       std::function<void(int)> onJoinRequested,
                       std::function<void()> onResumeRequested,
                       std::function<void()> onQuitRequested);

protected:
    void onLoaded(Rml::ElementDocument *document) override;

private:
    class RmlUiPanelCommunityListener;
    class ServerRowListener;
    class WebsiteLinkListener;
    class CommunityInfoListener;
    class DeleteDialogListener;
    class PasswordHintListener;
    class CredentialChangeListener;
    void handleSelection();
    void handleSelectionBlur();
    void handleAdd();
    void handleRefresh();
    void handleJoin();
    void handleResume();
    void handleQuit();
    void handleConfirmJoin(bool accepted);
    void handlePasswordHintDismiss();
    void handleErrorDialogClose();
    void handleDeleteConfirm(bool accepted);
    void showDeleteDialog();
    void handleCommunityInfoToggle();
    void clearAddStatus();
    void handleServerClick(int index);
    void updateServerDetails();
    std::string buildServerWebsite(const CommunityBrowserEntry &entry) const;
    std::string makeServerDetailsKey(const CommunityBrowserEntry &entry) const;
    bool isConnectedToEntry(const CommunityBrowserEntry &entry) const;
    bool hasActiveConnection() const;
    bool isLanSelected() const;

    bool loadUserConfig(nlohmann::json &out) const;
    bool saveUserConfig(const nlohmann::json &userConfig, std::string &error) const;
    void setNestedConfig(nlohmann::json &root, std::initializer_list<const char*> path, nlohmann::json value) const;
    void eraseNestedConfig(nlohmann::json &root, std::initializer_list<const char*> path) const;
    std::string communityKeyForIndex(int index) const;

    Rml::ElementDocument *document = nullptr;
    Rml::Element *addButton = nullptr;
    Rml::Element *refreshButton = nullptr;
    Rml::Element *serverList = nullptr;
    Rml::Element *selectElement = nullptr;
    Rml::Element *inputElement = nullptr;
    Rml::Element *usernameInput = nullptr;
    Rml::Element *passwordInput = nullptr;
    Rml::Element *passwordLabel = nullptr;
    Rml::Element *communityInfoButton = nullptr;
    Rml::Element *communityDeleteButton = nullptr;
    Rml::Element *detailTitle = nullptr;
    Rml::Element *joinButton = nullptr;
    Rml::Element *detailName = nullptr;
    Rml::Element *detailWebsite = nullptr;
    Rml::Element *detailOverview = nullptr;
    Rml::Element *detailDescription = nullptr;
    Rml::Element *detailScreenshot = nullptr;
    Rml::Element *detailServerSection = nullptr;
    Rml::Element *detailWebsiteSection = nullptr;
    Rml::Element *detailOverviewSection = nullptr;
    Rml::Element *detailDescriptionSection = nullptr;
    Rml::Element *detailScreenshotSection = nullptr;
    Rml::Element *detailLanInfoSection = nullptr;
    Rml::Element *detailLanInfoText = nullptr;
    Rml::Element *quitButton = nullptr;
    RmlUiModalDialog confirmDialog;
    RmlUiModalDialog errorDialog;
    RmlUiModalDialog deleteDialog;
    std::vector<ServerListOption> listOptions;
    std::vector<CommunityBrowserEntry> entries;
    int selectedIndex = -1;
    int selectedServerIndex = -1;
    int pendingJoinIndex = -1;
    bool suppressSelectionEvents = false;
    bool serverDescriptionLoading = false;
    std::string serverDescriptionLoadingKey;
    std::string serverDescriptionErrorKey;
    std::string serverDescriptionErrorText;
    bool showingCommunityInfo = false;
    std::string communityDetails;
    ConsoleInterface::ConnectionState connectionState{};
    bool passwordHintActive = false;
    std::string userConfigPath;
    std::string storedPasswordHash;
    std::optional<std::string> pendingDeleteListHost;
    std::function<void(int)> onSelectionChanged;
    std::function<void(const std::string &)> onAddRequested;
    std::function<void()> onRefreshRequested;
    std::function<void(int)> onServerSelectionChanged;
    std::function<void(int)> onJoinRequested;
    std::function<void()> onResumeRequested;
    std::function<void()> onQuitRequested;
    std::vector<std::unique_ptr<Rml::EventListener>> listeners;
};

} // namespace ui
