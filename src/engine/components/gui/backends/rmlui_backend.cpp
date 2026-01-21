#include "engine/components/gui/backends/rmlui_backend.hpp"

#include <RmlUi/Core.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/EventListener.h>
#include <RmlUi/Core/Factory.h>

#include <filesystem>
#include <functional>
#include <iomanip>
#include <sstream>
#include <unordered_map>
#include <unordered_map>

#include "engine/components/gui/rmlui_backend/RmlUi_Platform_GLFW.h"
#include "engine/components/gui/rmlui_backend/RmlUi_Renderer_GL3.h"
#include "engine/user_pointer.hpp"
#include "common/data_path_resolver.hpp"
#include "spdlog/spdlog.h"

namespace gui {
namespace {

class NullMainMenu final : public MainMenuInterface {
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

    std::optional<CommunityBrowserEntry> getSelectedEntry() const override {
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
    std::vector<CommunityBrowserEntry> entries;
    int selectedIndex = -1;
    std::vector<ServerListOption> listOptions;
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
    std::optional<CommunityBrowserSelection> pendingSelection;
    std::optional<int> pendingListSelection;
    std::optional<ServerListOption> pendingNewList;
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
};

NullMainMenu g_null_menu;
RmlUiBackend *g_backend_instance = nullptr;

int getModifierFlags(GLFWwindow *window) {
    int mods = 0;
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
        glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS) {
        mods |= GLFW_MOD_SHIFT;
    }
    if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ||
        glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS) {
        mods |= GLFW_MOD_CONTROL;
    }
    if (glfwGetKey(window, GLFW_KEY_LEFT_ALT) == GLFW_PRESS ||
        glfwGetKey(window, GLFW_KEY_RIGHT_ALT) == GLFW_PRESS) {
        mods |= GLFW_MOD_ALT;
    }
    if (glfwGetKey(window, GLFW_KEY_LEFT_SUPER) == GLFW_PRESS ||
        glfwGetKey(window, GLFW_KEY_RIGHT_SUPER) == GLFW_PRESS) {
        mods |= GLFW_MOD_SUPER;
    }
    return mods;
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

bool decodeNextUtf8(const std::string &utf8, std::size_t &offset, uint32_t &codepoint) {
    if (offset >= utf8.size()) {
        return false;
    }
    unsigned char c = static_cast<unsigned char>(utf8[offset]);
    if ((c & 0x80) == 0) {
        codepoint = c;
        offset += 1;
        return true;
    }
    int length = 0;
    if ((c & 0xE0) == 0xC0) {
        length = 2;
        codepoint = c & 0x1F;
    } else if ((c & 0xF0) == 0xE0) {
        length = 3;
        codepoint = c & 0x0F;
    } else if ((c & 0xF8) == 0xF0) {
        length = 4;
        codepoint = c & 0x07;
    } else {
        offset += 1;
        return false;
    }
    if (offset + length > utf8.size()) {
        offset += 1;
        return false;
    }
    for (int i = 1; i < length; ++i) {
        unsigned char cc = static_cast<unsigned char>(utf8[offset + i]);
        if ((cc & 0xC0) != 0x80) {
            offset += 1;
            return false;
        }
        codepoint = (codepoint << 6) | (cc & 0x3F);
    }
    offset += static_cast<std::size_t>(length);
    return true;
}

bool isEmojiCandidate(uint32_t cp) {
    if ((cp >= 0x1F000 && cp <= 0x1FAFF) ||
        (cp >= 0x2600 && cp <= 0x26FF) ||
        (cp >= 0x2700 && cp <= 0x27BF) ||
        (cp >= 0x2300 && cp <= 0x23FF) ||
        (cp >= 0x2B00 && cp <= 0x2BFF)) {
        return true;
    }
    switch (cp) {
        case 0x00A9:
        case 0x00AE:
        case 0x203C:
        case 0x2049:
        case 0x2122:
        case 0x2139:
        case 0x3030:
        case 0x200D:
        case 0xFE0F:
            return true;
        default:
            return false;
    }
}

bool isRegionalIndicator(uint32_t cp) {
    return cp >= 0x1F1E6 && cp <= 0x1F1FF;
}

bool isEmojiBase(uint32_t cp) {
    return isEmojiCandidate(cp) && cp != 0x200D && cp != 0xFE0F;
}

std::string codepointToHex(uint32_t cp) {
    std::ostringstream out;
    out << std::hex << std::nouppercase << cp;
    return out.str();
}

std::string buildTwemojiFilename(const std::vector<uint32_t> &sequence) {
    std::ostringstream out;
    for (std::size_t i = 0; i < sequence.size(); ++i) {
        if (i != 0) {
            out << '-';
        }
        out << codepointToHex(sequence[i]);
    }
    return out.str();
}

std::string renderTextWithTwemoji(const std::string &text) {
    std::string out;
    std::size_t offset = 0;
    while (offset < text.size()) {
        const std::size_t start = offset;
        uint32_t cp = 0;
        if (!decodeNextUtf8(text, offset, cp)) {
            out.append(escapeRmlText(text.substr(start, 1)));
            continue;
        }

        if (!isEmojiCandidate(cp) && !isRegionalIndicator(cp)) {
            out.append(escapeRmlText(text.substr(start, offset - start)));
            continue;
        }

        std::vector<uint32_t> sequence;
        sequence.push_back(cp);
        bool hasEmoji = isEmojiBase(cp) || isRegionalIndicator(cp);
        std::size_t seqOffset = offset;

        if (isRegionalIndicator(cp)) {
            uint32_t nextCp = 0;
            std::size_t lookahead = seqOffset;
            if (decodeNextUtf8(text, lookahead, nextCp) && isRegionalIndicator(nextCp)) {
                sequence.push_back(nextCp);
                seqOffset = lookahead;
                hasEmoji = true;
            }
        } else {
            while (seqOffset < text.size()) {
                uint32_t nextCp = 0;
                std::size_t lookahead = seqOffset;
                if (!decodeNextUtf8(text, lookahead, nextCp)) {
                    break;
                }
                if (nextCp == 0x200D || nextCp == 0xFE0F || isEmojiCandidate(nextCp) || isRegionalIndicator(nextCp)) {
                    sequence.push_back(nextCp);
                    if (isEmojiBase(nextCp) || isRegionalIndicator(nextCp)) {
                        hasEmoji = true;
                    }
                    seqOffset = lookahead;
                    continue;
                }
                break;
            }
        }

        if (!hasEmoji) {
            out.append(escapeRmlText(text.substr(start, offset - start)));
            continue;
        }

        const std::string fileName = buildTwemojiFilename(sequence);
        const std::filesystem::path imagePath = bz::data::Resolve("client/ui/emoji/twemoji/" + fileName + ".png");
        if (std::filesystem::exists(imagePath)) {
            const std::string srcPath = "emoji/twemoji/" + fileName + ".png";
            out.append("<img src=\"");
            out.append(srcPath);
            out.append("\" class=\"emoji\" />");
        } else {
            out.append(escapeRmlText(text.substr(start, seqOffset - start)));
        }

        offset = seqOffset;
    }
    return out;
}

struct RmlUiBackend::RmlUiState {
    SystemInterface_GLFW systemInterface;
    RenderInterface_GL3 renderInterface;
    Rml::Context *context = nullptr;
    Rml::ElementDocument *document = nullptr;
    std::function<void(GLFWwindow*, int, int, int, int)> previousKeyCallback;
    std::function<void(GLFWwindow*, int, int, int)> previousMouseCallback;
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
    std::string menuPath;
    bool reloadRequested = false;
    bool reloadArmed = false;
    bool hardReloadRequested = false;
    std::string regularFontPath;
    std::string emojiFontPath;
};

RmlUiBackend::RmlUiBackend(GLFWwindow *window) : window(window) {
    g_backend_instance = this;
    state = std::make_unique<RmlUiState>();
    state->systemInterface.SetWindow(window);

    Rml::SetSystemInterface(&state->systemInterface);
    Rml::SetRenderInterface(&state->renderInterface);

    Rml::String renderer_message;
    if (!RmlGL3::Initialize(&renderer_message)) {
        spdlog::error("RmlUi: failed to initialize GL3 renderer.");
        return;
    }
    spdlog::info("RmlUi: {}", renderer_message.c_str());

    if (!Rml::Initialise()) {
        spdlog::error("RmlUi: initialization failed.");
        return;
    }

    int fbWidth = 0;
    int fbHeight = 0;
    glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
    state->lastWidth = fbWidth;
    state->lastHeight = fbHeight;
    state->renderInterface.SetViewport(fbWidth, fbHeight);

    state->context = Rml::CreateContext("bz3", Rml::Vector2i(fbWidth, fbHeight));
    if (!state->context) {
        spdlog::error("RmlUi: failed to create context.");
        return;
    }

    float dpRatio = 1.0f;
    glfwGetWindowContentScale(window, &dpRatio, nullptr);
    state->lastDpRatio = dpRatio;
    state->context->SetDensityIndependentPixelRatio(dpRatio);

    const auto regularFontPath = bz::data::ResolveConfiguredAsset("hud.fonts.console.Regular.Font");
    if (!regularFontPath.empty()) {
        state->regularFontPath = regularFontPath.string();
        if (!Rml::LoadFontFace(state->regularFontPath)) {
            spdlog::warn("RmlUi: failed to load regular font '{}'.", state->regularFontPath);
        }
    }
    const auto emojiFontPath = bz::data::ResolveConfiguredAsset("hud.fonts.console.Emoji.Font");
    if (!emojiFontPath.empty()) {
        state->emojiFontPath = emojiFontPath.string();
        if (!Rml::LoadFontFace(state->emojiFontPath, true)) {
            spdlog::warn("RmlUi: failed to load emoji font '{}'.", state->emojiFontPath);
        }
    }

    const std::vector<std::string> fallbackKeys = {
        "hud.fonts.console.FallbackLatin.Font",
        "hud.fonts.console.FallbackArabic.Font",
        "hud.fonts.console.FallbackDevanagari.Font",
        "hud.fonts.console.FallbackCJK_JP.Font",
        "hud.fonts.console.FallbackCJK_KR.Font",
        "hud.fonts.console.FallbackCJK_SC.Font",
        "hud.fonts.console.FallbackCJK_TC.Font"
    };
    for (const auto &key : fallbackKeys) {
        const auto fallbackPath = bz::data::ResolveConfiguredAsset(key);
        if (!fallbackPath.empty()) {
            if (!Rml::LoadFontFace(fallbackPath.string(), true)) {
                spdlog::warn("RmlUi: failed to load fallback font '{}' ({})", fallbackPath.string(), key);
            }
        }
    }

    state->menuPath = bz::data::Resolve("client/ui/main_menu.rml").string();
    loadMenuDocument();

    auto *userPointer = static_cast<GLFWUserPointer *>(glfwGetWindowUserPointer(window));
    if (userPointer) {
        state->previousKeyCallback = userPointer->keyCallback;
        userPointer->keyCallback = [this](GLFWwindow *w, int key, int scancode, int action, int mods) {
            if (state && state->previousKeyCallback) {
                state->previousKeyCallback(w, key, scancode, action, mods);
            }
            if (action == GLFW_PRESS && key == GLFW_KEY_R && (mods & GLFW_MOD_CONTROL)) {
                state->reloadRequested = true;
                state->reloadArmed = true;
                if (mods & GLFW_MOD_SHIFT) {
                    state->hardReloadRequested = true;
                }
                return;
            }
            if (state && state->context) {
                RmlGLFW::ProcessKeyCallback(state->context, key, action, mods);
            }
        };

        state->previousMouseCallback = userPointer->mouseButtonCallback;
        userPointer->mouseButtonCallback = [this](GLFWwindow *w, int button, int action, int mods) {
            if (state && state->previousMouseCallback) {
                state->previousMouseCallback(w, button, action, mods);
            }
            if (state && state->context) {
                RmlGLFW::ProcessMouseButtonCallback(state->context, button, action, mods);
            }
        };
    }

    glfwSetCharCallback(window, [](GLFWwindow *w, unsigned int codepoint) {
        if (!g_backend_instance || !g_backend_instance->state || !g_backend_instance->state->context) {
            return;
        }
        RmlGLFW::ProcessCharCallback(g_backend_instance->state->context, codepoint);
    });

    glfwSetCursorEnterCallback(window, [](GLFWwindow *w, int entered) {
        if (!g_backend_instance || !g_backend_instance->state || !g_backend_instance->state->context) {
            return;
        }
        RmlGLFW::ProcessCursorEnterCallback(g_backend_instance->state->context, entered);
    });

    glfwSetCursorPosCallback(window, [](GLFWwindow *w, double xpos, double ypos) {
        if (!g_backend_instance || !g_backend_instance->state || !g_backend_instance->state->context) {
            return;
        }
        const int mods = getModifierFlags(w);
        RmlGLFW::ProcessCursorPosCallback(g_backend_instance->state->context, w, xpos, ypos, mods);
    });

    glfwSetScrollCallback(window, [](GLFWwindow *w, double, double yoffset) {
        if (!g_backend_instance || !g_backend_instance->state || !g_backend_instance->state->context) {
            return;
        }
        const int mods = getModifierFlags(w);
        RmlGLFW::ProcessScrollCallback(g_backend_instance->state->context, yoffset, mods);
    });

    glfwSetFramebufferSizeCallback(window, [](GLFWwindow *w, int width, int height) {
        if (!g_backend_instance || !g_backend_instance->state) {
            return;
        }
        g_backend_instance->state->lastWidth = width;
        g_backend_instance->state->lastHeight = height;
        g_backend_instance->state->renderInterface.SetViewport(width, height);
        if (g_backend_instance->state->context) {
            g_backend_instance->state->context->SetDimensions(Rml::Vector2i(width, height));
        }
    });

    glfwSetWindowContentScaleCallback(window, [](GLFWwindow *w, float xscale, float) {
        if (!g_backend_instance || !g_backend_instance->state || !g_backend_instance->state->context) {
            return;
        }
        g_backend_instance->state->lastDpRatio = xscale;
        g_backend_instance->state->context->SetDensityIndependentPixelRatio(xscale);
    });

    spdlog::info("GUI: RmlUi backend initialized.");
}

RmlUiBackend::~RmlUiBackend() {
    if (!state) {
        return;
    }
    g_backend_instance = nullptr;
    if (state->document) {
        state->document->Close();
        state->document = nullptr;
    }
    if (state->context) {
        Rml::RemoveContext(state->context->GetName());
        state->context = nullptr;
    }
    Rml::Shutdown();
    RmlGL3::Shutdown();
}

MainMenuInterface &RmlUiBackend::mainMenu() {
    return g_null_menu;
}

const MainMenuInterface &RmlUiBackend::mainMenu() const {
    return g_null_menu;
}

void RmlUiBackend::update() {
    if (!state || !state->context) {
        return;
    }

    int fbWidth = 0;
    int fbHeight = 0;
    glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
    if (fbWidth != state->lastWidth || fbHeight != state->lastHeight) {
        state->lastWidth = fbWidth;
        state->lastHeight = fbHeight;
        state->renderInterface.SetViewport(fbWidth, fbHeight);
        state->context->SetDimensions(Rml::Vector2i(fbWidth, fbHeight));
    }

    if (g_null_menu.isVisible()) {
        if (state->document && !state->document->IsVisible()) {
            state->document->Show();
        }
        if (!state->reloadRequested && !state->reloadArmed) {
            state->context->Update();
            state->renderInterface.BeginFrame();
            state->context->Render();
            state->renderInterface.EndFrame();
        }
    } else if (state->document && state->document->IsVisible()) {
        state->document->Hide();
    }

    if (state->reloadArmed) {
        state->reloadRequested = true;
        state->reloadArmed = false;
        return;
    }
    if (state->reloadRequested) {
        state->reloadRequested = false;
        loadMenuDocument();
    }
}

void RmlUiBackend::reloadFonts() {}

void RmlUiBackend::setScoreboardEntries(const std::vector<ScoreboardEntry> &) {}

void RmlUiBackend::setSpawnHint(const std::string &) {}

void RmlUiBackend::setRadarTextureId(unsigned int) {}

void RmlUiBackend::addConsoleLine(const std::string &, const std::string &) {}

std::string RmlUiBackend::getChatInputBuffer() const {
    return {};
}

void RmlUiBackend::clearChatInputBuffer() {}

void RmlUiBackend::focusChatInput() {}

bool RmlUiBackend::getChatInputFocus() const {
    return false;
}

void RmlUiBackend::displayDeathScreen(bool) {}

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

void RmlUiBackend::loadMenuDocument() {
    if (!state || !state->context) {
        return;
    }

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
    state->emojiMarkupCache.clear();

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
    state->document = state->context->LoadDocument(state->menuPath);
    if (!state->document) {
        spdlog::error("RmlUi: failed to load main menu RML from '{}'.", state->menuPath);
        return;
    }

    state->document->Show();
    state->contentElement = state->document->GetElementById("tab-content");
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
        setActiveTab(defaultTabKey.empty() ? state->tabs.begin()->first : defaultTabKey);
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
    const std::string markup = renderTextWithTwemoji(text);
    auto [inserted, _] = state->emojiMarkupCache.emplace(text, markup);
    return inserted->second;
}

} // namespace gui
