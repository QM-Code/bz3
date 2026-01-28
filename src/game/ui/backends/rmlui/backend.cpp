#include "ui/backends/rmlui/backend.hpp"

#include <RmlUi/Core.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/EventListener.h>
#include <RmlUi/Core/Factory.h>
#include <RmlUi/Core/Input.h>
#include <RmlUi/Core/SystemInterface.h>

#include <filesystem>
#include <fstream>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <optional>
#include <chrono>
#include <cmath>

#if defined(BZ3_RENDER_BACKEND_BGFX)
#include "ui/backends/rmlui/platform/RmlUi_Renderer_BGFX.h"
#elif defined(BZ3_RENDER_BACKEND_DILIGENT)
#include "ui/backends/rmlui/platform/RmlUi_Renderer_Diligent.h"
#else
#error "RmlUi backend requires BGFX or Diligent renderer."
#endif
#include "ui/backends/rmlui/console/emoji_utils.hpp"
#include "ui/backends/rmlui/translate.hpp"
#include "platform/window.hpp"
#include "common/config_helpers.hpp"
#include "common/i18n.hpp"
#include "ui/backends/rmlui/hud/hud.hpp"
#include "ui/backends/rmlui/console/console.hpp"
#include "ui/backends/rmlui/console/panels/panel_community.hpp"
#include "ui/backends/rmlui/console/panels/panel_documentation.hpp"
#include "ui/backends/rmlui/console/panels/panel_settings.hpp"
#include "ui/backends/rmlui/console/panels/panel_start_server.hpp"
#include "ui/backends/rmlui/console/panels/panel_themes.hpp"
#include "common/data_path_resolver.hpp"
#include "spdlog/spdlog.h"

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
                            const std::string &saltIn) override {
        (void)communityHost;
        username = usernameIn;
        storedPasswordHash = passhash;
        salt = saltIn;
    }

    void setCommunityStatus(const std::string &text, MessageTone tone) override {
        communityStatusText = text;
        communityStatusTone = tone;
    }

    std::optional<ui::CommunityBrowserEntry> getSelectedEntry() const override {
        if (selectedIndex < 0 || selectedIndex >= static_cast<int>(entries.size())) {
            return std::nullopt;
        }
        return entries[static_cast<std::size_t>(selectedIndex)];
    }

    bool consumeRefreshRequest() override {
        return consumeOptional(refreshRequested);
    }

    void setScanning(bool scanningIn) override {
        scanning = scanningIn;
    }

    void setUserConfigPath(const std::string &path) override {
        userConfigPath = path;
    }

    bool consumeFontReloadRequest() override {
        return consumeOptional(fontReloadRequested);
    }

    bool consumeKeybindingsReloadRequest() override {
        return false;
    }

    void setConnectionState(const ConnectionState &state) override {
        connectionState = state;
    }

    ConnectionState getConnectionState() const override {
        return connectionState;
    }

    bool consumeQuitRequest() override {
        return consumeOptional(quitRequested);
    }

    void showErrorDialog(const std::string &) override {}

private:
    template <typename T>
    std::optional<T> consumeOptional(std::optional<T> &value) {
        if (!value) {
            return std::nullopt;
        }
        std::optional<T> out = value;
        value.reset();
        return out;
    }

    bool consumeOptional(bool &value) {
        const bool out = value;
        value = false;
        return out;
    }

    bool visible = false;
    std::vector<ui::CommunityBrowserEntry> entries;
    int selectedIndex = -1;
    std::vector<ui::ServerListOption> listOptions;
    int listSelectedIndex = -1;
    std::string statusText;
    bool statusIsError = false;
    std::string communityDetailsText;
    std::string communityStatusText;
    MessageTone communityStatusTone = MessageTone::Notice;
    std::string serverDescriptionLoadingKey;
    bool serverDescriptionLoading = false;
    std::string serverDescriptionErrorKey;
    std::string serverDescriptionErrorText;
    std::optional<ui::CommunityBrowserSelection> pendingSelection;
    std::optional<int> pendingListSelection;
    std::optional<ui::ServerListOption> pendingNewList;
    std::optional<std::string> pendingDeleteListHost;
    std::string listStatusText;
    bool listStatusIsError = false;
    std::string username;
    std::string password;
    std::string storedPasswordHash;
    std::string salt;
    std::string newListHost;
    bool scanning = false;
    std::string userConfigPath;
    bool fontReloadRequested = false;
    bool refreshRequested = false;
    bool quitRequested = false;
    ConnectionState connectionState{};
};

class SystemInterface_Platform final : public Rml::SystemInterface {
public:
    void SetWindow(platform::Window *windowIn) { window = windowIn; }

    double GetElapsedTime() override {
        using namespace std::chrono;
        const auto now = steady_clock::now();
        return duration<double>(now - startTime).count();
    }

    void SetMouseCursor(const Rml::String &cursor_name) override {
        if (!window) {
            return;
        }
        const bool visible = (cursor_name != "none");
        window->setCursorVisible(visible);
    }

    void SetClipboardText(const Rml::String &text) override {
        if (!window) {
            return;
        }
        window->setClipboardText(text);
    }

    void GetClipboardText(Rml::String &text) override {
        if (!window) {
            text.clear();
            return;
        }
        text = window->getClipboardText();
    }

private:
    platform::Window *window = nullptr;
    std::chrono::steady_clock::time_point startTime = std::chrono::steady_clock::now();
};

Rml::Input::KeyIdentifier toRmlKey(platform::Key key) {
    switch (key) {
        case platform::Key::A: return Rml::Input::KI_A;
        case platform::Key::B: return Rml::Input::KI_B;
        case platform::Key::C: return Rml::Input::KI_C;
        case platform::Key::D: return Rml::Input::KI_D;
        case platform::Key::E: return Rml::Input::KI_E;
        case platform::Key::F: return Rml::Input::KI_F;
        case platform::Key::G: return Rml::Input::KI_G;
        case platform::Key::H: return Rml::Input::KI_H;
        case platform::Key::I: return Rml::Input::KI_I;
        case platform::Key::J: return Rml::Input::KI_J;
        case platform::Key::K: return Rml::Input::KI_K;
        case platform::Key::L: return Rml::Input::KI_L;
        case platform::Key::M: return Rml::Input::KI_M;
        case platform::Key::N: return Rml::Input::KI_N;
        case platform::Key::O: return Rml::Input::KI_O;
        case platform::Key::P: return Rml::Input::KI_P;
        case platform::Key::Q: return Rml::Input::KI_Q;
        case platform::Key::R: return Rml::Input::KI_R;
        case platform::Key::S: return Rml::Input::KI_S;
        case platform::Key::T: return Rml::Input::KI_T;
        case platform::Key::U: return Rml::Input::KI_U;
        case platform::Key::V: return Rml::Input::KI_V;
        case platform::Key::W: return Rml::Input::KI_W;
        case platform::Key::X: return Rml::Input::KI_X;
        case platform::Key::Y: return Rml::Input::KI_Y;
        case platform::Key::Z: return Rml::Input::KI_Z;
        case platform::Key::Num0: return Rml::Input::KI_0;
        case platform::Key::Num1: return Rml::Input::KI_1;
        case platform::Key::Num2: return Rml::Input::KI_2;
        case platform::Key::Num3: return Rml::Input::KI_3;
        case platform::Key::Num4: return Rml::Input::KI_4;
        case platform::Key::Num5: return Rml::Input::KI_5;
        case platform::Key::Num6: return Rml::Input::KI_6;
        case platform::Key::Num7: return Rml::Input::KI_7;
        case platform::Key::Num8: return Rml::Input::KI_8;
        case platform::Key::Num9: return Rml::Input::KI_9;
        case platform::Key::F1: return Rml::Input::KI_F1;
        case platform::Key::F2: return Rml::Input::KI_F2;
        case platform::Key::F3: return Rml::Input::KI_F3;
        case platform::Key::F4: return Rml::Input::KI_F4;
        case platform::Key::F5: return Rml::Input::KI_F5;
        case platform::Key::F6: return Rml::Input::KI_F6;
        case platform::Key::F7: return Rml::Input::KI_F7;
        case platform::Key::F8: return Rml::Input::KI_F8;
        case platform::Key::F9: return Rml::Input::KI_F9;
        case platform::Key::F10: return Rml::Input::KI_F10;
        case platform::Key::F11: return Rml::Input::KI_F11;
        case platform::Key::F12: return Rml::Input::KI_F12;
        case platform::Key::F13: return Rml::Input::KI_F13;
        case platform::Key::F14: return Rml::Input::KI_F14;
        case platform::Key::F15: return Rml::Input::KI_F15;
        case platform::Key::F16: return Rml::Input::KI_F16;
        case platform::Key::F17: return Rml::Input::KI_F17;
        case platform::Key::F18: return Rml::Input::KI_F18;
        case platform::Key::F19: return Rml::Input::KI_F19;
        case platform::Key::F20: return Rml::Input::KI_F20;
        case platform::Key::F21: return Rml::Input::KI_F21;
        case platform::Key::F22: return Rml::Input::KI_F22;
        case platform::Key::F23: return Rml::Input::KI_F23;
        case platform::Key::F24: return Rml::Input::KI_F24;
        case platform::Key::Space: return Rml::Input::KI_SPACE;
        case platform::Key::Escape: return Rml::Input::KI_ESCAPE;
        case platform::Key::Enter: return Rml::Input::KI_RETURN;
        case platform::Key::Tab: return Rml::Input::KI_TAB;
        case platform::Key::Backspace: return Rml::Input::KI_BACK;
        case platform::Key::Left: return Rml::Input::KI_LEFT;
        case platform::Key::Right: return Rml::Input::KI_RIGHT;
        case platform::Key::Up: return Rml::Input::KI_UP;
        case platform::Key::Down: return Rml::Input::KI_DOWN;
        case platform::Key::LeftBracket: return Rml::Input::KI_OEM_4;
        case platform::Key::RightBracket: return Rml::Input::KI_OEM_6;
        case platform::Key::Minus: return Rml::Input::KI_OEM_MINUS;
        case platform::Key::Equal: return Rml::Input::KI_OEM_PLUS;
        case platform::Key::Apostrophe: return Rml::Input::KI_OEM_7;
        case platform::Key::GraveAccent: return Rml::Input::KI_OEM_3;
        case platform::Key::LeftShift: return Rml::Input::KI_LSHIFT;
        case platform::Key::RightShift: return Rml::Input::KI_RSHIFT;
        case platform::Key::LeftControl: return Rml::Input::KI_LCONTROL;
        case platform::Key::RightControl: return Rml::Input::KI_RCONTROL;
        case platform::Key::LeftAlt: return Rml::Input::KI_LMENU;
        case platform::Key::RightAlt: return Rml::Input::KI_RMENU;
        case platform::Key::LeftSuper: return Rml::Input::KI_LMETA;
        case platform::Key::RightSuper: return Rml::Input::KI_RMETA;
        case platform::Key::Home: return Rml::Input::KI_HOME;
        case platform::Key::End: return Rml::Input::KI_END;
        case platform::Key::PageUp: return Rml::Input::KI_PRIOR;
        case platform::Key::PageDown: return Rml::Input::KI_NEXT;
        case platform::Key::Insert: return Rml::Input::KI_INSERT;
        case platform::Key::Delete: return Rml::Input::KI_DELETE;
        case platform::Key::CapsLock: return Rml::Input::KI_CAPITAL;
        case platform::Key::NumLock: return Rml::Input::KI_NUMLOCK;
        case platform::Key::ScrollLock: return Rml::Input::KI_SCROLL;
        default: return Rml::Input::KI_UNKNOWN;
    }
}

int toRmlMods(const platform::Modifiers &mods) {
    int out = 0;
    if (mods.control) {
        out |= Rml::Input::KM_CTRL;
    }
    if (mods.shift) {
        out |= Rml::Input::KM_SHIFT;
    }
    if (mods.alt) {
        out |= Rml::Input::KM_ALT;
    }
    if (mods.super) {
        out |= Rml::Input::KM_META;
    }
    return out;
}

int currentRmlMods(platform::Window *window) {
    if (!window) {
        return 0;
    }
    platform::Modifiers mods;
    mods.shift = window->isKeyDown(platform::Key::LeftShift) || window->isKeyDown(platform::Key::RightShift);
    mods.control = window->isKeyDown(platform::Key::LeftControl) || window->isKeyDown(platform::Key::RightControl);
    mods.alt = window->isKeyDown(platform::Key::LeftAlt) || window->isKeyDown(platform::Key::RightAlt);
    mods.super = window->isKeyDown(platform::Key::LeftSuper) || window->isKeyDown(platform::Key::RightSuper);
    return toRmlMods(mods);
}

int toRmlMouseButton(platform::MouseButton button) {
    switch (button) {
        case platform::MouseButton::Left: return 0;
        case platform::MouseButton::Right: return 1;
        case platform::MouseButton::Middle: return 2;
        case platform::MouseButton::Button4: return 3;
        case platform::MouseButton::Button5: return 4;
        case platform::MouseButton::Button6: return 5;
        case platform::MouseButton::Button7: return 6;
        case platform::MouseButton::Button8: return 7;
        default: return 0;
    }
}

class TabClickListener final : public Rml::EventListener {
public:
    TabClickListener(class RmlUiBackend *backendIn, std::string tabKeyIn)
        : backend(backendIn), tabKey(std::move(tabKeyIn)) {}

    void ProcessEvent(Rml::Event &) override {
        if (backend) {
            backend->setActiveTab(tabKey);
        }
    }

private:
    class RmlUiBackend *backend = nullptr;
    std::string tabKey;
};


} // namespace

std::string escapeRmlText(const std::string &text) {
    std::string out;
    out.reserve(text.size());
    for (char ch : text) {
        switch (ch) {
            case '&': out.append("&amp;"); break;
            case '<': out.append("&lt;"); break;
            case '>': out.append("&gt;"); break;
            case '"': out.append("&quot;"); break;
            case '\'': out.append("&#39;"); break;
            default: out.push_back(ch); break;
        }
    }
    return out;
}


struct RmlUiBackend::RmlUiState {
    SystemInterface_Platform systemInterface;
#if defined(BZ3_RENDER_BACKEND_BGFX)
    RenderInterface_BGFX renderInterface;
#elif defined(BZ3_RENDER_BACKEND_DILIGENT)
    RenderInterface_Diligent renderInterface;
#endif
    Rml::Context *context = nullptr;
    Rml::ElementDocument *document = nullptr;
    Rml::Element *bodyElement = nullptr;
    int lastWidth = 0;
    int lastHeight = 0;
    float lastDpRatio = 1.0f;
    std::string activeTab;
    std::unordered_map<std::string, Rml::Element*> tabs;
    std::unordered_map<std::string, std::string> tabLabels;
    std::unordered_map<std::string, Rml::Element*> tabPanels;
    Rml::Element *contentElement = nullptr;
    std::vector<std::unique_ptr<Rml::EventListener>> tabListeners;
    std::unordered_map<std::string, std::string> emojiMarkupCache;
    std::vector<std::unique_ptr<ui::RmlUiPanel>> panels;
    std::unordered_set<std::string> loadedFontFiles;
    std::string consolePath;
    std::string hudPath;
    bool reloadRequested = false;
    bool reloadArmed = false;
    bool hardReloadRequested = false;
    std::optional<std::string> pendingLanguage;
    std::string regularFontPath;
    std::string emojiFontPath;
    std::unique_ptr<ui::RmlUiHud> hud;
    bool showFps = false;
    double fpsLastTime = 0.0;
    double fpsValue = 0.0;
    int fpsFrames = 0;
    bool outputVisible = false;
};

RmlUiBackend::RmlUiBackend(platform::Window &windowRefIn) : windowRef(&windowRefIn) {
    state = std::make_unique<RmlUiState>();
    consoleView = std::make_unique<ui::RmlUiConsole>();
    state->systemInterface.SetWindow(windowRef);

    Rml::SetSystemInterface(&state->systemInterface);
    Rml::SetRenderInterface(&state->renderInterface);
#if defined(BZ3_RENDER_BACKEND_BGFX)
    if (!state->renderInterface) {
        spdlog::error("RmlUi: failed to initialize bgfx renderer.");
        return;
    }
    spdlog::info("RmlUi: bgfx renderer initialized.");
#elif defined(BZ3_RENDER_BACKEND_DILIGENT)
    if (!state->renderInterface) {
        spdlog::error("RmlUi: failed to initialize Diligent renderer.");
        return;
    }
    spdlog::info("RmlUi: Diligent renderer initialized.");
#endif

    if (!Rml::Initialise()) {
        spdlog::error("RmlUi: initialization failed.");
        return;
    }

    int fbWidth = 0;
    int fbHeight = 0;
    if (windowRef) { windowRef->getFramebufferSize(fbWidth, fbHeight); }
    state->lastWidth = fbWidth;
    state->lastHeight = fbHeight;
    state->renderInterface.SetViewport(fbWidth, fbHeight);

    state->context = Rml::CreateContext("bz3", Rml::Vector2i(fbWidth, fbHeight));
    if (!state->context) {
        spdlog::error("RmlUi: failed to create context.");
        return;
    }

    float dpRatio = 1.0f;
    if (windowRef) { dpRatio = windowRef->getContentScale(); }
    state->lastDpRatio = dpRatio;
    state->context->SetDensityIndependentPixelRatio(dpRatio);

    loadConfiguredFonts(bz::i18n::Get().language());

    state->consolePath = bz::data::Resolve("client/ui/console.rml").string();
    state->hudPath = bz::data::Resolve("client/ui/hud.rml").string();
    state->hud = std::make_unique<ui::RmlUiHud>();
    state->showFps = bz::data::ReadBoolConfig({"debug.ShowFPS"}, false);
    state->fpsLastTime = state->systemInterface.GetElapsedTime();
    auto communityPanel = std::make_unique<ui::RmlUiPanelCommunity>();
    auto *communityPanelPtr = communityPanel.get();
    state->panels.emplace_back(std::move(communityPanel));
    auto settingsPanel = std::make_unique<ui::RmlUiPanelSettings>();
    auto *settingsPanelPtr = settingsPanel.get();
    state->panels.emplace_back(std::move(settingsPanel));
    this->settingsPanel = settingsPanelPtr;
    settingsPanelPtr->setLanguageCallback([this](const std::string &language) {
        if (!state) {
            return;
        }
        state->pendingLanguage = language;
        state->reloadRequested = false;
        state->reloadArmed = true;
    });
    state->panels.emplace_back(std::make_unique<ui::RmlUiPanelDocumentation>());
    auto startServerPanel = std::make_unique<ui::RmlUiPanelStartServer>();
    auto *startServerPanelPtr = startServerPanel.get();
    state->panels.emplace_back(std::move(startServerPanel));
    state->panels.emplace_back(std::make_unique<ui::RmlUiPanelThemes>());
    consoleView->attachCommunityPanel(communityPanelPtr);
    consoleView->attachSettingsPanel(settingsPanelPtr);
    consoleView->attachStartServerPanel(startServerPanelPtr);
    communityPanelPtr->bindCallbacks(
        [this](int index) {
            if (consoleView) {
                consoleView->onCommunitySelection(index);
            }
        },
        [this](const std::string &host) {
            if (consoleView) {
                consoleView->onCommunityAddRequested(host);
            }
        },
        [this]() {
            if (consoleView) {
                consoleView->onRefreshRequested();
            }
        },
        [this](int index) {
            if (consoleView) {
                consoleView->onServerSelection(index);
            }
        },
        [this](int index) {
            if (consoleView) {
                consoleView->onJoinRequested(index);
            }
        },
        [this]() {
            if (consoleView) {
                consoleView->hide();
            }
        },
        [this]() {
            if (consoleView) {
                consoleView->onQuitRequested();
            }
        }
    );
    loadConsoleDocument();
    loadHudDocument();

    spdlog::info("UiSystem: RmlUi backend initialized.");
}

RmlUiBackend::~RmlUiBackend() {
    if (!state) {
        return;
    }
    if (state->document) {
        state->document->Close();
        state->document = nullptr;
    }
    if (state->hud) {
        state->hud->unload();
    }
    if (state->context) {
        Rml::RemoveContext(state->context->GetName());
        state->context = nullptr;
    }
    Rml::Shutdown();
}

ui::ConsoleInterface &RmlUiBackend::console() {
    return *consoleView;
}

const ui::ConsoleInterface &RmlUiBackend::console() const {
    return *consoleView;
}

void RmlUiBackend::handleEvents(const std::vector<platform::Event> &events) {
    if (!state || !state->context) {
        return;
    }

    const bool consoleVisible = consoleView && consoleView->isVisible();
    const bool hudVisible = state->hud && state->hud->isVisible();

    for (const auto &event : events) {
        switch (event.type) {
            case platform::EventType::KeyDown: {
                int mods = toRmlMods(event.mods);
                if (mods == 0) {
                    mods = currentRmlMods(windowRef);
                }
                if (event.key == platform::Key::R && (mods & Rml::Input::KM_CTRL)) {
                    state->reloadRequested = true;
                    state->reloadArmed = true;
                    if (mods & Rml::Input::KM_SHIFT) {
                        state->hardReloadRequested = true;
                    }
                    break;
                }
                if (!isUiInputEnabled()) {
                    break;
                }
                state->context->ProcessKeyDown(toRmlKey(event.key), mods);
                break;
            }
            case platform::EventType::KeyUp: {
                if (!isUiInputEnabled()) {
                    break;
                }
                int mods = toRmlMods(event.mods);
                if (mods == 0) {
                    mods = currentRmlMods(windowRef);
                }
                state->context->ProcessKeyUp(toRmlKey(event.key), mods);
                break;
            }
            case platform::EventType::TextInput: {
                if (!isUiInputEnabled()) {
                    break;
                }
                if (state->hud && state->hud->consumeSuppressNextChatChar()) {
                    break;
                }
                state->context->ProcessTextInput(static_cast<Rml::Character>(event.codepoint));
                break;
            }
            case platform::EventType::MouseButtonDown: {
                if (!(consoleVisible || hudVisible)) {
                    break;
                }
                int mods = toRmlMods(event.mods);
                if (mods == 0) {
                    mods = currentRmlMods(windowRef);
                }
                state->context->ProcessMouseButtonDown(toRmlMouseButton(event.mouseButton), mods);
                break;
            }
            case platform::EventType::MouseButtonUp: {
                if (!(consoleVisible || hudVisible)) {
                    break;
                }
                int mods = toRmlMods(event.mods);
                if (mods == 0) {
                    mods = currentRmlMods(windowRef);
                }
                state->context->ProcessMouseButtonUp(toRmlMouseButton(event.mouseButton), mods);
                break;
            }
            case platform::EventType::MouseMove: {
                if (!(consoleVisible || hudVisible)) {
                    break;
                }
                int mods = toRmlMods(event.mods);
                if (mods == 0) {
                    mods = currentRmlMods(windowRef);
                }
                const int x = static_cast<int>(std::lround(event.x));
                const int y = static_cast<int>(std::lround(event.y));
                state->context->ProcessMouseMove(x, y, mods);
                break;
            }
            case platform::EventType::MouseScroll: {
                if (!(consoleVisible || hudVisible)) {
                    break;
                }
                int mods = toRmlMods(event.mods);
                if (mods == 0) {
                    mods = currentRmlMods(windowRef);
                }
                state->context->ProcessMouseWheel(-static_cast<float>(event.scrollY), mods);
                break;
            }
            case platform::EventType::WindowFocus: {
                if (!event.focused) {
                    state->context->ProcessMouseLeave();
                }
                break;
            }
            case platform::EventType::WindowResize: {
                state->lastWidth = event.width;
                state->lastHeight = event.height;
                state->renderInterface.SetViewport(event.width, event.height);
                state->context->SetDimensions(Rml::Vector2i(event.width, event.height));
                break;
            }
            case platform::EventType::WindowClose: {
                state->context->ProcessMouseLeave();
                break;
            }
            default:
                break;
        }
    }
}

bool RmlUiBackend::isUiInputEnabled() const {
    if (consoleView && consoleView->isVisible()) {
        return true;
    }
    return state && state->hud && state->hud->isChatFocused();
}

void RmlUiBackend::update() {
    if (!state || !state->context) {
        return;
    }

    if (renderBridge && state->hud) {
        state->hud->setRadarTexture(renderBridge->getRadarTexture());
    }

    int fbWidth = 0;
    int fbHeight = 0;
    if (windowRef) { windowRef->getFramebufferSize(fbWidth, fbHeight); }
    if (fbWidth != state->lastWidth || fbHeight != state->lastHeight) {
        state->lastWidth = fbWidth;
        state->lastHeight = fbHeight;
        state->renderInterface.SetViewport(fbWidth, fbHeight);
        state->context->SetDimensions(Rml::Vector2i(fbWidth, fbHeight));
    }

    float dpRatio = 1.0f;
    if (windowRef) {
        dpRatio = windowRef->getContentScale();
    }
    if (dpRatio != state->lastDpRatio) {
        state->lastDpRatio = dpRatio;
        state->context->SetDensityIndependentPixelRatio(dpRatio);
    }

    const bool consoleVisible = consoleView && consoleView->isVisible();
    if (consoleVisible) {
        if (state->document && !state->document->IsVisible()) {
            state->document->Show();
        }
        if (state->hud) {
            state->hud->hide();
        }
        if (!state->bodyElement && state->document) {
            state->bodyElement = state->document->GetElementById("main-body");
        }
        if (state->bodyElement) {
            const bool inGame = consoleView->getConnectionState().connected;
            state->bodyElement->SetClass("in-game", inGame);
        }
    } else {
        if (state->document && state->document->IsVisible()) {
            state->document->Hide();
        }
        if (state->hud) {
            state->hud->show();
        }
    }

    const bool anyVisible = (state->document && state->document->IsVisible())
        || (state->hud && state->hud->isVisible());
    state->outputVisible = anyVisible;
    if (anyVisible && !state->reloadRequested && !state->reloadArmed) {
        if (consoleVisible) {
            for (const auto &panel : state->panels) {
                panel->update();
            }
        } else if (state->hud) {
            state->hud->update();
            if (state->showFps) {
                state->fpsFrames += 1;
                const double now = state->systemInterface.GetElapsedTime();
                const double elapsed = now - state->fpsLastTime;
                if (elapsed >= 0.25) {
                    state->fpsValue = state->fpsFrames / elapsed;
                    state->fpsFrames = 0;
                    state->fpsLastTime = now;
                }
                state->hud->setFpsVisible(true);
                state->hud->setFpsValue(static_cast<float>(state->fpsValue));
            } else {
                state->hud->setFpsVisible(false);
            }
        }
        state->context->Update();
        state->renderInterface.BeginFrame();
        state->context->Render();
        state->renderInterface.EndFrame();
    }

    if (state->reloadArmed) {
        state->reloadRequested = true;
        state->reloadArmed = false;
        return;
    }
    if (state->reloadRequested) {
        state->reloadRequested = false;
        if (state->pendingLanguage) {
            bz::i18n::Get().loadLanguage(*state->pendingLanguage);
            state->pendingLanguage.reset();
        }
        loadConsoleDocument();
        loadHudDocument();
    }
}

void RmlUiBackend::reloadFonts() {
    if (!state) {
        return;
    }
    loadConsoleDocument();
    loadHudDocument();
}

void RmlUiBackend::setScoreboardEntries(const std::vector<ScoreboardEntry> &entries) {
    if (!state || !state->hud) {
        return;
    }
    state->hud->setScoreboardEntries(entries);
}

void RmlUiBackend::setSpawnHint(const std::string &hint) {
    if (!state || !state->hud) {
        return;
    }
    state->hud->setDialogText(hint);
}

void RmlUiBackend::addConsoleLine(const std::string &playerName, const std::string &line) {
    if (!state || !state->hud) {
        return;
    }
    std::string displayName = playerName;
    if (!displayName.empty() && displayName.front() != '[') {
        displayName = "[" + displayName + "]";
    }
    const std::string fullLine = displayName.empty() ? line : (displayName + " " + line);
    state->hud->addChatLine(fullLine);
}

std::string RmlUiBackend::getChatInputBuffer() const {
    if (!state || !state->hud) {
        return {};
    }
    return state->hud->getSubmittedChatInput();
}

void RmlUiBackend::clearChatInputBuffer() {
    if (!state || !state->hud) {
        return;
    }
    state->hud->clearSubmittedChatInput();
}

void RmlUiBackend::focusChatInput() {
    if (!state || !state->hud) {
        return;
    }
    state->hud->focusChatInput();
}

bool RmlUiBackend::getChatInputFocus() const {
    if (!state || !state->hud) {
        return false;
    }
    return state->hud->isChatFocused();
}

void RmlUiBackend::displayDeathScreen(bool show) {
    if (!state || !state->hud) {
        return;
    }
    state->hud->showDialog(show);
}

bool RmlUiBackend::consumeKeybindingsReloadRequest() {
    return consoleView && consoleView->consumeKeybindingsReloadRequest();
}

void RmlUiBackend::setRenderBridge(const ui::RenderBridge *bridge) {
    renderBridge = bridge;
}

ui::RenderOutput RmlUiBackend::getRenderOutput() const {
    if (!state) {
        return {};
    }
    if (!state->outputVisible) {
        return {};
    }
    ui::RenderOutput out;
    out.texture.id = static_cast<uint64_t>(state->renderInterface.GetOutputTextureId());
    const int outputWidth = state->renderInterface.GetOutputWidth();
    const int outputHeight = state->renderInterface.GetOutputHeight();
    out.texture.width = outputWidth > 0 ? static_cast<uint32_t>(outputWidth) : 0u;
    out.texture.height = outputHeight > 0 ? static_cast<uint32_t>(outputHeight) : 0u;
    out.visible = state->outputVisible;
    return out;
}

float RmlUiBackend::getRenderBrightness() const {
    return settingsPanel ? settingsPanel->getRenderBrightness() : 1.0f;
}

void RmlUiBackend::setActiveTab(const std::string &tabKey) {
    if (!state) {
        return;
    }
    auto it = state->tabs.find(tabKey);
    if (it == state->tabs.end()) {
        return;
    }

    state->activeTab = tabKey;
    for (const auto &entry : state->tabs) {
        entry.second->SetClass("active", entry.first == tabKey);
    }
    for (const auto &entry : state->tabPanels) {
        entry.second->SetClass("active", entry.first == tabKey);
    }

    if (state->contentElement && state->tabPanels.find(tabKey) == state->tabPanels.end()) {
        std::string label = tabKey;
        auto labelIt = state->tabLabels.find(tabKey);
        if (labelIt != state->tabLabels.end() && !labelIt->second.empty()) {
            label = labelIt->second;
        }
        const std::string &labelMarkup = cachedTwemojiMarkup(label);
        state->contentElement->SetInnerRML(
            "<div style=\"padding: 8px 0;\">" + labelMarkup + " panel</div>");
    }
}

void RmlUiBackend::loadConfiguredFonts(const std::string &language) {
    if (!state) {
        return;
    }
    auto loadFont = [&](const std::filesystem::path &path, bool fallback) {
        if (path.empty()) {
            return;
        }
        const std::string pathStr = path.string();
        if (!state->loadedFontFiles.insert(pathStr).second) {
            return;
        }
        if (!Rml::LoadFontFace(pathStr, fallback)) {
            spdlog::warn("RmlUi: failed to load font '{}' (fallback={}).", pathStr, fallback);
        }
    };

    state->regularFontPath.clear();
    state->emojiFontPath.clear();

    const auto regularFontPath = bz::data::ResolveConfiguredAsset("hud.fonts.console.Regular.Font");
    if (!regularFontPath.empty()) {
        state->regularFontPath = regularFontPath.string();
        loadFont(regularFontPath, false);
    }
    const auto titleFontPath = bz::data::ResolveConfiguredAsset("hud.fonts.console.Title.Font");
    loadFont(titleFontPath, false);
    const auto headingFontPath = bz::data::ResolveConfiguredAsset("hud.fonts.console.Heading.Font");
    loadFont(headingFontPath, false);
    const auto buttonFontPath = bz::data::ResolveConfiguredAsset("hud.fonts.console.Button.Font");
    loadFont(buttonFontPath, false);

    const auto emojiFontPath = bz::data::ResolveConfiguredAsset("hud.fonts.console.Emoji.Font");
    if (!emojiFontPath.empty()) {
        state->emojiFontPath = emojiFontPath.string();
        loadFont(emojiFontPath, true);
    }

    if (const auto *extras = bz::data::ConfigValue("assets.hud.fonts.console.Extras")) {
        if (extras->is_array()) {
            for (const auto &entry : *extras) {
                if (!entry.is_string()) {
                    continue;
                }
                const std::string extra = entry.get<std::string>();
                std::filesystem::path extraPath;
                if (extra.rfind("client/", 0) == 0 || extra.rfind("common/", 0) == 0) {
                    extraPath = bz::data::Resolve(extra);
                } else {
                    extraPath = bz::data::Resolve(std::filesystem::path("client") / extra);
                }
                loadFont(extraPath, false);
            }
        }
    }

    const std::string lang = language;
    if (lang == "ru") {
        loadFont(bz::data::ResolveConfiguredAsset("hud.fonts.console.FallbackLatin.Font"), true);
    } else if (lang == "ar") {
        loadFont(bz::data::ResolveConfiguredAsset("hud.fonts.console.FallbackArabic.Font"), true);
    } else if (lang == "hi") {
        loadFont(bz::data::ResolveConfiguredAsset("hud.fonts.console.FallbackDevanagari.Font"), true);
    } else if (lang == "jp") {
        loadFont(bz::data::ResolveConfiguredAsset("hud.fonts.console.FallbackCJK_JP.Font"), true);
    } else if (lang == "ko") {
        loadFont(bz::data::ResolveConfiguredAsset("hud.fonts.console.FallbackCJK_KR.Font"), true);
    } else if (lang == "zh") {
        loadFont(bz::data::ResolveConfiguredAsset("hud.fonts.console.FallbackCJK_SC.Font"), true);
    }
}

void RmlUiBackend::loadConsoleDocument() {
    if (!state || !state->context) {
        return;
    }

    const std::string previousTab = state->activeTab;
    state->reloadRequested = false;
    state->reloadArmed = false;
    if (state->document) {
        state->document->Close();
        state->document = nullptr;
        // Allow RmlUi to detach listeners and destroy elements before we clear them.
        state->context->Update();
    }

    state->tabs.clear();
    state->tabLabels.clear();
    state->tabListeners.clear();
    state->tabPanels.clear();
    state->contentElement = nullptr;
    state->bodyElement = nullptr;
    state->emojiMarkupCache.clear();

    loadConfiguredFonts(bz::i18n::Get().language());

    Rml::Factory::ClearStyleSheetCache();
    Rml::Factory::ClearTemplateCache();
    if (state->hardReloadRequested) {
        state->hardReloadRequested = false;
        if (!state->regularFontPath.empty()) {
            Rml::LoadFontFace(state->regularFontPath);
        }
        if (!state->emojiFontPath.empty()) {
            Rml::LoadFontFace(state->emojiFontPath, true);
        }
    }
    state->document = state->context->LoadDocument(state->consolePath);
    if (!state->document) {
        spdlog::error("RmlUi: failed to load console RML from '{}'.", state->consolePath);
        return;
    }
    ui::rmlui::ApplyTranslations(state->document, bz::i18n::Get());

    state->document->Show();
    state->bodyElement = state->document->GetElementById("main-body");
    state->contentElement = state->document->GetElementById("tab-content");
    for (const auto &panel : state->panels) {
        panel->load(state->document);
    }
    Rml::ElementList tabElements;
    std::string defaultTabKey;
    state->document->GetElementsByClassName(tabElements, "tab");
    for (auto *element : tabElements) {
        if (!element) {
            continue;
        }
        const std::string elementId = element->GetId();
        std::string tabKey = elementId;
        const std::string prefix = "tab-";
        if (tabKey.rfind(prefix, 0) == 0) {
            tabKey = tabKey.substr(prefix.size());
        }
        if (tabKey.empty()) {
            continue;
        }
        const std::string labelRaw = element->GetInnerRML();
        state->tabs[tabKey] = element;
        state->tabLabels[tabKey] = labelRaw;
        element->SetInnerRML(cachedTwemojiMarkup(labelRaw));
        if (defaultTabKey.empty() && element->IsClassSet("default")) {
            defaultTabKey = tabKey;
        }
        auto listener = std::make_unique<TabClickListener>(this, tabKey);
        element->AddEventListener("click", listener.get());
        state->tabListeners.emplace_back(std::move(listener));
    }
    for (const auto &entry : state->tabs) {
        const std::string panelId = "panel-" + entry.first;
        if (auto *panel = state->document->GetElementById(panelId)) {
            state->tabPanels[entry.first] = panel;
        }
    }
    if (!state->tabs.empty()) {
        if (!previousTab.empty() && state->tabs.find(previousTab) != state->tabs.end()) {
            setActiveTab(previousTab);
        } else {
            setActiveTab(defaultTabKey.empty() ? state->tabs.begin()->first : defaultTabKey);
        }
    }
}

void RmlUiBackend::loadHudDocument() {
    if (!state || !state->context) {
        return;
    }

    if (state->hud) {
        state->hud->load(state->context, state->hudPath,
            [this](const std::string &text) -> const std::string & {
                return cachedTwemojiMarkup(text);
            });
    }
}

const std::string &RmlUiBackend::cachedTwemojiMarkup(const std::string &text) {
    if (!state) {
        static const std::string empty;
        return empty;
    }
    auto it = state->emojiMarkupCache.find(text);
    if (it != state->emojiMarkupCache.end()) {
        return it->second;
    }
    const std::string markup = ui::renderTextWithTwemoji(text);
    auto [inserted, _] = state->emojiMarkupCache.emplace(text, markup);
    return inserted->second;
}

} // namespace ui
