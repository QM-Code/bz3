#include "ui/backend.hpp"

#include "spdlog/spdlog.h"

#if defined(BZ3_UI_BACKEND_IMGUI)
#include "ui/frontends/imgui/backend.hpp"
#elif defined(BZ3_UI_BACKEND_RMLUI)
#include "ui/frontends/rmlui/backend.hpp"
#else
#error "BZ3 UI backend not set. Define BZ3_UI_BACKEND_IMGUI or BZ3_UI_BACKEND_RMLUI."
#endif

namespace ui_backend {

namespace {

class NullConsole final : public ui::ConsoleInterface {
public:
    void show(const std::vector<ui::CommunityBrowserEntry> &entriesIn) override {
        entries = entriesIn;
        visible = true;
    }

    void setEntries(const std::vector<ui::CommunityBrowserEntry> &entriesIn) override {
        entries = entriesIn;
    }

    void setListOptions(const std::vector<ui::ServerListOption> &options, int selectedIndexIn) override {
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

    std::optional<ui::CommunityBrowserSelection> consumeSelection() override {
        return consumeOptional(pendingSelection);
    }

    std::optional<int> consumeListSelection() override {
        return consumeOptional(pendingListSelection);
    }

    std::optional<ui::ServerListOption> consumeNewListRequest() override {
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
        (void)tone;
        communityStatusText = text;
    }

    std::optional<ui::CommunityBrowserEntry> getSelectedEntry() const override {
        if (entries.empty()) {
            return std::nullopt;
        }
        return entries.front();
    }

    bool consumeRefreshRequest() override {
        return consumeOptional(pendingRefreshRequest).value_or(false);
    }

    void setScanning(bool scanningIn) override {
        scanning = scanningIn;
    }

    void setUserConfigPath(const std::string &path) override {
        userConfigPath = path;
    }

    bool consumeFontReloadRequest() override {
        return consumeOptional(pendingFontReload).value_or(false);
    }

    bool consumeKeybindingsReloadRequest() override {
        return consumeOptional(pendingKeybindingsReload).value_or(false);
    }

    void setConnectionState(const ConnectionState &stateIn) override {
        connectionState = stateIn;
    }

    ConnectionState getConnectionState() const override {
        return connectionState;
    }

    bool consumeQuitRequest() override {
        return consumeOptional(pendingQuitRequest).value_or(false);
    }

    void showErrorDialog(const std::string &message) override {
        lastErrorDialog = message;
    }

private:
    template <typename T>
    std::optional<T> consumeOptional(std::optional<T> &value) {
        if (!value.has_value()) {
            return std::nullopt;
        }
        auto out = value;
        value.reset();
        return out;
    }

    bool visible = false;
    bool statusIsError = false;
    bool listStatusIsError = false;
    bool serverDescriptionLoading = false;
    bool scanning = false;
    int listSelectedIndex = 0;

    std::string statusText;
    std::string communityDetailsText;
    std::string serverDescriptionLoadingKey;
    std::string serverDescriptionErrorKey;
    std::string serverDescriptionErrorText;
    std::string listStatusText;
    std::string newListHost;
    std::string username;
    std::string password;
    std::string storedPasswordHash;
    std::string communityStatusText;
    std::string userConfigPath;
    std::string lastErrorDialog;

    std::optional<ui::CommunityBrowserSelection> pendingSelection;
    std::optional<int> pendingListSelection;
    std::optional<ui::ServerListOption> pendingNewList;
    std::optional<std::string> pendingDeleteListHost;
    std::optional<bool> pendingRefreshRequest;
    std::optional<bool> pendingFontReload;
    std::optional<bool> pendingKeybindingsReload;
    std::optional<bool> pendingQuitRequest;

    ConnectionState connectionState{};
    std::vector<ui::CommunityBrowserEntry> entries;
    std::vector<ui::ServerListOption> listOptions;
};

class NullBackend final : public Backend {
public:
    ui::ConsoleInterface &console() override { return consoleImpl; }
    const ui::ConsoleInterface &console() const override { return consoleImpl; }
    void handleEvents(const std::vector<platform::Event> &events) override { (void)events; }
    void update() override {}
    void reloadFonts() override {}
    void setScoreboardEntries(const std::vector<ScoreboardEntry> &entries) override { (void)entries; }
    void setSpawnHint(const std::string &hint) override { (void)hint; }
    void addConsoleLine(const std::string &playerName, const std::string &line) override {
        (void)playerName;
        (void)line;
    }
    std::string getChatInputBuffer() const override { return {}; }
    void clearChatInputBuffer() override {}
    void focusChatInput() override {}
    bool getChatInputFocus() const override { return false; }
    void displayDeathScreen(bool show) override { (void)show; }
    bool consumeKeybindingsReloadRequest() override { return false; }
    void setRenderBridge(const ui::RenderBridge *bridge) override { (void)bridge; }
    ui::RenderOutput getRenderOutput() const override { return {}; }
    float getRenderBrightness() const override { return 1.0f; }

private:
    NullConsole consoleImpl;
};

} // namespace

std::unique_ptr<Backend> CreateUiBackend(platform::Window &window) {
    if (const char* noUi = std::getenv("BZ3_NO_UI"); noUi && noUi[0] != '\0') {
        spdlog::warn("UiSystem: UI disabled via BZ3_NO_UI");
        return std::make_unique<NullBackend>();
    }
#if defined(BZ3_UI_BACKEND_IMGUI)
    return std::make_unique<ImGuiBackend>(window);
#elif defined(BZ3_UI_BACKEND_RMLUI)
    return std::make_unique<RmlUiBackend>(window);
#endif
}

} // namespace ui_backend
