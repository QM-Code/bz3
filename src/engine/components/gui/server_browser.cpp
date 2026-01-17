#include "engine/components/gui/server_browser.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <imgui.h>
#include <imgui_internal.h>
#include <spdlog/spdlog.h>
#include <string>
#include <unordered_map>
#include <vector>
#include <curl/curl.h>
#include <imgui_impl_opengl3_loader.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace {
using stbi_uc = Effekseer::stbi_uc;
using Effekseer::stbi_load_from_memory;
using Effekseer::stbi_image_free;
}

#include "common/data_path_resolver.hpp"
#include "common/curl_global.hpp"

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

    uint16_t configuredServerPort() {
        if (const auto configured = bz::data::ConfigValueUInt16("network.ServerPort")) {
            return *configured;
        }
        return 0;
    }

    std::string configuredServerPortLabel() {
        if (const auto label = bz::data::ConfigValueString("network.ServerPort")) {
            return *label;
        }
        return std::to_string(configuredServerPort());
    }

    uint16_t applyPortFallback(uint16_t candidate) {
        if (candidate != 0) {
            return candidate;
        }
        return configuredServerPort();
    }

    size_t CurlWriteToVector(char *ptr, size_t size, size_t nmemb, void *userdata) {
        auto *buffer = static_cast<std::vector<unsigned char> *>(userdata);
        const size_t total = size * nmemb;
        buffer->insert(buffer->end(), reinterpret_cast<unsigned char *>(ptr), reinterpret_cast<unsigned char *>(ptr) + total);
        return total;
    }

    std::string normalizedHost(const std::string &host) {
        if (host.empty()) {
            return {};
        }
        std::string trimmed = host;
        while (!trimmed.empty() && trimmed.back() == '/') {
            trimmed.pop_back();
        }
        return trimmed;
    }
}

namespace gui {

void ServerBrowserView::initializeFonts(ImGuiIO &io) {
    const auto regularFontPath = bz::data::ResolveConfiguredAsset("fonts.console.Regular");
    const std::string regularFontPathStr = regularFontPath.string();
    regularFont = io.Fonts->AddFontFromFileTTF(
        regularFontPathStr.c_str(),
        20.0f
    );

    if (!regularFont) {
        spdlog::warn("Failed to load console regular font for server browser ({}).", regularFontPathStr);
    }

    const auto headingFontPath = bz::data::ResolveConfiguredAsset("fonts.console.Heading");
    const std::string headingFontPathStr = headingFontPath.string();
    headingFont = io.Fonts->AddFontFromFileTTF(
        headingFontPathStr.c_str(),
        28.0f
    );

    if (!headingFont) {
        spdlog::warn("Failed to load console heading font for server browser ({}).", headingFontPathStr);
    }

    const auto buttonFontPath = bz::data::ResolveConfiguredAsset("fonts.console.Button");
    const std::string buttonFontPathStr = buttonFontPath.string();
    buttonFont = io.Fonts->AddFontFromFileTTF(
        buttonFontPathStr.c_str(),
        18.0f
    );

    if (!buttonFont) {
        spdlog::warn("Failed to load console button font for server browser ({}).", buttonFontPathStr);
    }
}

void ServerBrowserView::draw(ImGuiIO &io) {
    if (!visible) {
        return;
    }

    processThumbnailUploads();

    const bool pushedRegularFont = (regularFont != nullptr);
    if (pushedRegularFont) {
        ImGui::PushFont(regularFont);
    }

    const bool hasHeadingFont = (headingFont != nullptr);
    const bool hasButtonFont = (buttonFont != nullptr);

    const ImVec2 windowSize(1200.0f, 680.0f);
    const ImVec2 windowPos(
        (io.DisplaySize.x - windowSize.x) * 0.5f,
        (io.DisplaySize.y - windowSize.y) * 0.5f
    );

    ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(windowSize, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.95f);

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoMove;

    if (hasHeadingFont) {
        ImGui::PushFont(headingFont);
    }
    ImGui::Begin("Server Browser", nullptr, flags);
    if (hasHeadingFont) {
        ImGui::PopFont();
    }

    ImVec2 contentAvail = ImGui::GetContentRegionAvail();
    const ImGuiStyle &style = ImGui::GetStyle();
    const float minDetailWidth = 300.0f;
    const float minListWidth = 280.0f;
    float maxListWidth = std::max(minListWidth, contentAvail.x - minDetailWidth - style.ItemSpacing.x);
    float listPanelWidth = std::max(320.0f, contentAvail.x * 0.5f);
    listPanelWidth = std::clamp(listPanelWidth, minListWidth, maxListWidth);

    ImGui::BeginChild("ServerBrowserListPane", ImVec2(listPanelWidth, 0), false);
    const MessageColors messageColors = getMessageColors();

    auto formatListLabel = [](const ServerListOption &option) {
        if (!option.name.empty()) {
            return option.name;
        }
        if (!option.host.empty()) {
            return option.host;
        }
        return std::string("Unnamed list");
    };

    if (listOptions.empty() || listSelectedIndex < 0) {
        ImGui::TextDisabled("Add a server list below to fetch public servers.");
    } else {
        listSelectedIndex = std::clamp(
            listSelectedIndex,
            0,
            static_cast<int>(listOptions.size()) - 1);

        const auto &currentOption = listOptions[listSelectedIndex];
        std::string comboLabel = formatListLabel(currentOption);
        if (ImGui::BeginCombo("##ServerListSelector", comboLabel.c_str())) {
            for (int i = 0; i < static_cast<int>(listOptions.size()); ++i) {
                const auto &option = listOptions[i];
                std::string optionLabel = formatListLabel(option);
                bool selected = (i == listSelectedIndex);
                if (ImGui::Selectable(optionLabel.c_str(), selected)) {
                    if (!selected) {
                        listSelectedIndex = i;
                        pendingListSelection = i;
                    }
                }
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
    }

    ImGui::Spacing();

    const float refreshButtonWidth = ImGui::CalcTextSize("Refresh").x + style.FramePadding.x * 2.0f;

    const ImGuiTableFlags tableFlags =
        ImGuiTableFlags_Resizable |
        ImGuiTableFlags_RowBg |
        ImGuiTableFlags_BordersOuter |
        ImGuiTableFlags_ScrollY;

    const float tableHeight = 260.0f;
    const float playerColumnWidth = 120.0f;

    if (ImGui::BeginTable("##ServerBrowserPresets", 2, tableFlags, ImVec2(-1.0f, tableHeight))) {
        ImGui::TableSetupColumn("##ServerListColumn", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn("##PlayerCountColumn", ImGuiTableColumnFlags_WidthFixed, playerColumnWidth);

        ImGui::TableNextRow(ImGuiTableRowFlags_Headers);

        const char *serversHeadingLabel = "Servers";

        ImGui::TableSetColumnIndex(0);
        if (hasHeadingFont) {
            ImGui::PushFont(headingFont);
        }
        ImGui::TextUnformatted(serversHeadingLabel);
        if (hasHeadingFont) {
            ImGui::PopFont();
        }

        ImGui::TableSetColumnIndex(1);
        const float headerStartX = ImGui::GetCursorPosX();
        const float headerStartY = ImGui::GetCursorPosY();
        const float headerColumnWidth = ImGui::GetColumnWidth();
        float buttonX = headerStartX + headerColumnWidth - refreshButtonWidth;
        float lineBottom = ImGui::GetCursorPosY();

        ImGui::SetCursorPos(ImVec2(buttonX, headerStartY));
        if (hasButtonFont) {
            ImGui::PushFont(buttonFont);
        }
        if (ImGui::Button("Refresh")) {
            refreshRequested = true;
        }
        if (hasButtonFont) {
            ImGui::PopFont();
        }
        lineBottom = std::max(lineBottom, ImGui::GetCursorPosY());
        ImGui::SetCursorPosY(lineBottom);

        if (entries.empty()) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            if (!communityStatusText.empty()) {
                ImVec4 statusColor = messageColors.notice;
                if (communityStatusTone == MessageTone::Error) {
                    statusColor = messageColors.error;
                } else if (communityStatusTone == MessageTone::Pending) {
                    statusColor = messageColors.pending;
                }
                ImGui::TextColored(statusColor, "%s", communityStatusText.c_str());
            } else {
                ImGui::TextDisabled("No saved servers yet.");
            }
        } else {
            for (int i = 0; i < static_cast<int>(entries.size()); ++i) {
                const auto &entry = entries[i];
                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                ImGui::PushID(i);
                bool selected = (selectedIndex == i);
                std::string selectableLabel = entry.label + "##server_row_" + std::to_string(i);
                if (ImGui::Selectable(selectableLabel.c_str(), selected,
                        ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick)) {
                    selectedIndex = i;
                    if (ImGui::IsMouseDoubleClicked(0)) {
                        pendingSelection = ServerBrowserSelection{
                            entry.host,
                            entry.port,
                            true,
                            entry.sourceHost,
                            entry.worldName
                        };
                        statusText.clear();
                        statusIsError = false;
                    }
                }

                ImGui::TableSetColumnIndex(1);
                const bool hasActive = entry.activePlayers >= 0;
                const bool hasMax = entry.maxPlayers >= 0;
                if (hasActive || hasMax) {
                    std::string activeText = hasActive ? std::to_string(entry.activePlayers) : std::string("?");
                    std::string maxText = hasMax ? std::to_string(entry.maxPlayers) : std::string("?");

                    const float columnCursorX = ImGui::GetCursorPosX();
                    const float columnWidth = ImGui::GetColumnWidth();
                    float textWidth = ImGui::CalcTextSize((activeText + " / " + maxText).c_str()).x;
                    float targetX = columnCursorX + columnWidth - style.CellPadding.x - textWidth;
                    if (targetX < columnCursorX) {
                        targetX = columnCursorX;
                    }
                    float targetY = ImGui::GetCursorPosY();
                    ImGui::SetCursorPos(ImVec2(targetX, targetY));

                    ImFont *countFont = regularFont ? regularFont : ImGui::GetFont();
                    if (countFont) {
                        ImGui::PushFont(countFont);
                    }

                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.60f, 0.80f, 0.40f, 1.0f));
                    ImGui::TextUnformatted(activeText.c_str());
                    ImGui::PopStyleColor();

                    ImGui::SameLine(0.0f, 0.0f);
                    ImGui::TextUnformatted(" / ");
                    ImGui::SameLine(0.0f, 0.0f);
                    ImGui::TextUnformatted(maxText.c_str());

                    if (countFont) {
                        ImGui::PopFont();
                    }
                }

                ImGui::PopID();
            }
        }

        ImGui::EndTable();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (!statusText.empty()) {
        ImVec4 color = statusIsError ? messageColors.error : messageColors.pending;
        ImGui::TextColored(color, "%s", statusText.c_str());
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
    }

    ImGui::Text("Player identity");
    ImGui::InputText("Username", usernameBuffer.data(), usernameBuffer.size());
    ImGui::InputText("Password", passwordBuffer.data(), passwordBuffer.size(), ImGuiInputTextFlags_Password);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::Text("Custom server");
    ImGui::InputText("Address (host:port)", addressBuffer.data(), addressBuffer.size());

    bool joinCustomClicked = false;
    if (hasButtonFont) {
        ImGui::PushFont(buttonFont);
    }
    if (ImGui::Button("Join Custom")) {
        joinCustomClicked = true;
    }
    if (hasButtonFont) {
        ImGui::PopFont();
    }
    if (joinCustomClicked) {
        std::string addressValue = trimCopy(addressBuffer.data());
        if (addressValue.empty()) {
            customStatusText = "Enter a server address before joining.";
            customStatusIsError = true;
        } else {
            auto colonPos = addressValue.find_last_of(':');
            if (colonPos == std::string::npos) {
                const std::string exampleAddress = "localhost:" + configuredServerPortLabel();
                customStatusText = "Use the format host:port (example: " + exampleAddress + ").";
                customStatusIsError = true;
            } else {
                std::string hostPart = trimCopy(addressValue.substr(0, colonPos));
                std::string portPart = trimCopy(addressValue.substr(colonPos + 1));

                if (hostPart.empty()) {
                    customStatusText = "Hostname cannot be empty.";
                    customStatusIsError = true;
                } else if (portPart.empty()) {
                    customStatusText = "Port cannot be empty.";
                    customStatusIsError = true;
                } else {
                    try {
                        int portValue = std::stoi(portPart);
                        if (portValue < 1 || portValue > 65535) {
                            customStatusText = "Ports must be between 1 and 65535.";
                            customStatusIsError = true;
                        } else {
                            pendingSelection = ServerBrowserSelection{
                                hostPart,
                                static_cast<uint16_t>(portValue),
                                false,
                                std::string{},
                                std::string{}
                            };
                            customStatusText.clear();
                            customStatusIsError = false;
                        }
                    } catch (...) {
                        customStatusText = "Port must be a valid number.";
                        customStatusIsError = true;
                    }
                }
            }
        }
    }

    if (!customStatusText.empty()) {
        ImGui::Spacing();
        ImVec4 color = customStatusIsError ? messageColors.error : messageColors.action;
        ImGui::TextColored(color, "%s", customStatusText.c_str());
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::Text("Add server list");
    ImGui::InputText("Community host", listUrlBuffer.data(), listUrlBuffer.size());

    bool saveListClicked = false;
    if (hasButtonFont) {
        ImGui::PushFont(buttonFont);
    }
    if (ImGui::Button("Save Server List")) {
        saveListClicked = true;
    }
    if (hasButtonFont) {
        ImGui::PopFont();
    }
    if (saveListClicked) {
        std::string urlValue(listUrlBuffer.data());
        if (urlValue.empty()) {
            listStatusText = "Enter a host before saving.";
            listStatusIsError = true;
        } else {
            listStatusText.clear();
            listStatusIsError = false;
            pendingNewList = ServerListOption{ std::string{}, urlValue };
        }
    }

    if (!listStatusText.empty()) {
        ImGui::Spacing();
        ImVec4 listColor = listStatusIsError ? messageColors.error : messageColors.action;
        ImGui::TextColored(listColor, "%s", listStatusText.c_str());
    }

    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("ServerBrowserDetailsPane", ImVec2(0, 0), true);
    if (hasHeadingFont) {
        ImGui::PushFont(headingFont);
    }
    ImGui::TextUnformatted("Server Details");
    if (hasHeadingFont) {
        ImGui::PopFont();
    }
    ImGui::SameLine();
    const float joinButtonWidth = ImGui::CalcTextSize("Join").x + style.FramePadding.x * 2.0f;
    const float joinButtonOffset = std::max(0.0f, ImGui::GetContentRegionAvail().x - joinButtonWidth);
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + joinButtonOffset);
    bool joinSelectedClicked = false;
    if (hasButtonFont) {
        ImGui::PushFont(buttonFont);
    }
    if (ImGui::Button("Join")) {
        joinSelectedClicked = true;
    }
    if (hasButtonFont) {
        ImGui::PopFont();
    }
    if (joinSelectedClicked) {
        if (selectedIndex >= 0 && selectedIndex < static_cast<int>(entries.size())) {
            const auto &entry = entries[selectedIndex];
            pendingSelection = ServerBrowserSelection{
                entry.host,
                entry.port,
                true,
                entry.sourceHost,
                entry.worldName
            };
            statusText.clear();
            statusIsError = false;
        } else {
            statusText = "Choose a server from the list first.";
            statusIsError = true;
        }
    }
    ImGui::Separator();

    const ServerBrowserEntry *selectedEntry = nullptr;
    if (selectedIndex >= 0 && selectedIndex < static_cast<int>(entries.size())) {
        selectedEntry = &entries[selectedIndex];
    }

    if (!selectedEntry) {
        ImGui::TextDisabled("Select a server to see more information.");
    } else {
        ImGui::TextWrapped("%s", selectedEntry->label.c_str());
        ImGui::Spacing();

        const std::string &displayHost = selectedEntry->displayHost.empty() ? selectedEntry->host : selectedEntry->displayHost;
        ImGui::Text("Host: %s", displayHost.c_str());
        ImGui::Text("Port: %u", selectedEntry->port);

        if (selectedEntry->activePlayers >= 0) {
            if (selectedEntry->maxPlayers >= 0) {
                ImGui::Text("Players: %d/%d", selectedEntry->activePlayers, selectedEntry->maxPlayers);
            } else {
                ImGui::Text("Players: %d", selectedEntry->activePlayers);
            }
        } else if (selectedEntry->maxPlayers >= 0) {
            ImGui::Text("Capacity: %d", selectedEntry->maxPlayers);
        }

        if (!selectedEntry->gameMode.empty()) {
            ImGui::Text("Mode: %s", selectedEntry->gameMode.c_str());
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextUnformatted("Description");
        if (!selectedEntry->longDescription.empty()) {
            ImGui::TextWrapped("%s", selectedEntry->longDescription.c_str());
        } else {
            ImGui::TextDisabled("No description provided.");
        }

        if (!selectedEntry->screenshotId.empty() && !selectedEntry->sourceHost.empty()) {
            std::string hostBase = normalizedHost(selectedEntry->sourceHost);
            std::string thumbnailUrl = hostBase + "/uploads/" + selectedEntry->screenshotId + "_thumb.jpg";

            if (auto *thumb = getOrLoadThumbnail(thumbnailUrl)) {
                if (thumb->textureId != 0 && thumb->width > 0 && thumb->height > 0) {
                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::TextUnformatted("Screenshot");

                    const float maxWidth = ImGui::GetContentRegionAvail().x;
                    const float maxHeight = 220.0f;
                    float scale = 1.0f;
                    scale = std::min(scale, maxWidth / static_cast<float>(thumb->width));
                    scale = std::min(scale, maxHeight / static_cast<float>(thumb->height));
                    if (scale <= 0.0f) {
                        scale = 1.0f;
                    }

                    ImVec2 imageSize(
                        static_cast<float>(thumb->width) * scale,
                        static_cast<float>(thumb->height) * scale);

                    ImGui::Image(static_cast<ImTextureID>(static_cast<intptr_t>(thumb->textureId)), imageSize);
                } else if (thumb->failed) {
                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::TextDisabled("Screenshot unavailable.");
                } else if (thumb->loading) {
                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::TextDisabled("Loading screenshot...");
                }
            }
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextUnformatted("Plugins");
        if (!selectedEntry->flags.empty()) {
            for (const auto &flag : selectedEntry->flags) {
                ImGui::BulletText("%s", flag.c_str());
            }
        } else {
            ImGui::TextDisabled("No plugins reported.");
        }
    }

    ImGui::EndChild();

    ImGui::End();

    if (pushedRegularFont) {
        ImGui::PopFont();
    }
}

void ServerBrowserView::show(const std::vector<ServerBrowserEntry> &newEntries,
              const std::string &defaultHost,
              uint16_t defaultPort) {
    visible = true;
    setEntries(newEntries);
    pendingSelection.reset();
    statusText = "Select a server to connect or enter your own.";
    statusIsError = false;
    customStatusText.clear();
    customStatusIsError = false;
    pendingListSelection.reset();
    pendingNewList.reset();
    listStatusText.clear();
    listStatusIsError = false;
    communityStatusText.clear();
    communityStatusTone = MessageTone::Notice;
    clearPassword();
    resetBuffers(defaultHost, defaultPort);
}

ServerBrowserView::~ServerBrowserView() {
    stopThumbnailWorker();
    clearThumbnails();
}

void ServerBrowserView::setEntries(const std::vector<ServerBrowserEntry> &newEntries) {
    entries = newEntries;
    if (entries.empty()) {
        selectedIndex = -1;
    } else if (selectedIndex < 0) {
        selectedIndex = 0;
    } else if (selectedIndex >= static_cast<int>(entries.size())) {
        selectedIndex = static_cast<int>(entries.size()) - 1;
    }
}

void ServerBrowserView::setListOptions(const std::vector<ServerListOption> &options, int selectedIndexIn) {
    listOptions = options;
    if (listOptions.empty()) {
        listSelectedIndex = -1;
        pendingListSelection.reset();
        return;
    }

    if (selectedIndexIn < 0) {
        listSelectedIndex = 0;
    } else if (selectedIndexIn >= static_cast<int>(listOptions.size())) {
        listSelectedIndex = static_cast<int>(listOptions.size()) - 1;
    } else {
        listSelectedIndex = selectedIndexIn;
    }
}

void ServerBrowserView::hide() {
    visible = false;
    statusText.clear();
    statusIsError = false;
    customStatusText.clear();
    customStatusIsError = false;
    pendingSelection.reset();
    pendingListSelection.reset();
    pendingNewList.reset();
    refreshRequested = false;
    scanning = false;
    listStatusText.clear();
    listStatusIsError = false;
    communityStatusText.clear();
    communityStatusTone = MessageTone::Notice;
    clearPassword();
    stopThumbnailWorker();
    clearThumbnails();
}

bool ServerBrowserView::isVisible() const {
    return visible;
}

void ServerBrowserView::setStatus(const std::string &text, bool isErrorMessage) {
    statusText = text;
    statusIsError = isErrorMessage;
}

void ServerBrowserView::setCustomStatus(const std::string &text, bool isErrorMessage) {
    customStatusText = text;
    customStatusIsError = isErrorMessage;
}

std::optional<ServerBrowserSelection> ServerBrowserView::consumeSelection() {
    if (!pendingSelection.has_value()) {
        return std::nullopt;
    }

    auto selection = pendingSelection;
    pendingSelection.reset();
    return selection;
}

std::optional<int> ServerBrowserView::consumeListSelection() {
    if (!pendingListSelection.has_value()) {
        return std::nullopt;
    }

    auto selection = pendingListSelection;
    pendingListSelection.reset();
    return selection;
}

std::optional<ServerListOption> ServerBrowserView::consumeNewListRequest() {
    if (!pendingNewList.has_value()) {
        return std::nullopt;
    }

    auto request = pendingNewList;
    pendingNewList.reset();
    return request;
}

void ServerBrowserView::setListStatus(const std::string &text, bool isErrorMessage) {
    listStatusText = text;
    listStatusIsError = isErrorMessage;
}

void ServerBrowserView::clearNewListInputs() {
    listUrlBuffer.fill(0);
}

void ServerBrowserView::setCommunityStatus(const std::string &text, MessageTone tone) {
    communityStatusText = text;
    communityStatusTone = tone;
}

std::string ServerBrowserView::getUsername() const {
    return trimCopy(usernameBuffer.data());
}

std::string ServerBrowserView::getPassword() const {
    return std::string(passwordBuffer.data());
}

void ServerBrowserView::clearPassword() {
    passwordBuffer.fill(0);
}

bool ServerBrowserView::consumeRefreshRequest() {
    if (!refreshRequested) {
        return false;
    }
    refreshRequested = false;
    return true;
}

void ServerBrowserView::setScanning(bool isScanning) {
    scanning = isScanning;
}

void ServerBrowserView::resetBuffers(const std::string &defaultHost, uint16_t defaultPort) {
    std::string hostValue = defaultHost.empty() ? std::string("localhost") : defaultHost;
    uint16_t portValue = applyPortFallback(defaultPort);

    addressBuffer.fill(0);
    std::snprintf(
        addressBuffer.data(),
        addressBuffer.size(),
        "%s:%u",
        hostValue.c_str(),
        portValue);
}

ThumbnailTexture *ServerBrowserView::getOrLoadThumbnail(const std::string &url) {
    if (url.empty()) {
        return nullptr;
    }

    auto it = thumbnailCache.find(url);
    if (it != thumbnailCache.end()) {
        if (it->second.textureId != 0 || it->second.failed || it->second.loading) {
            return &it->second;
        }
    }

    ThumbnailTexture &entry = thumbnailCache[url];
    if (!entry.loading && entry.textureId == 0 && !entry.failed) {
        entry.loading = true;
        queueThumbnailRequest(url);
    }
    return &entry;
}

void ServerBrowserView::clearThumbnails() {
    for (auto &[url, thumb] : thumbnailCache) {
        if (thumb.textureId != 0) {
            glDeleteTextures(1, &thumb.textureId);
        }
    }
    thumbnailCache.clear();
}

ServerBrowserView::MessageColors ServerBrowserView::getMessageColors() const {
    MessageColors colors;
    colors.error = ImVec4(0.93f, 0.36f, 0.36f, 1.0f);
    colors.notice = ImVec4(0.90f, 0.80f, 0.30f, 1.0f);
    colors.action = ImVec4(0.60f, 0.80f, 0.40f, 1.0f);
    colors.pending = ImVec4(0.35f, 0.70f, 0.95f, 1.0f);
    return colors;
}

void ServerBrowserView::startThumbnailWorker() {
    if (thumbnailWorker.joinable()) {
        return;
    }

    thumbnailWorkerStop = false;
    thumbnailWorker = std::thread(&ServerBrowserView::thumbnailWorkerProc, this);
}

void ServerBrowserView::stopThumbnailWorker() {
    {
        std::lock_guard<std::mutex> lock(thumbnailMutex);
        thumbnailWorkerStop = true;
        thumbnailRequests.clear();
        thumbnailInFlight.clear();
        thumbnailResults.clear();
    }
    thumbnailCv.notify_all();
    if (thumbnailWorker.joinable()) {
        thumbnailWorker.join();
    }
}

void ServerBrowserView::queueThumbnailRequest(const std::string &url) {
    startThumbnailWorker();
    std::lock_guard<std::mutex> lock(thumbnailMutex);
    if (!thumbnailInFlight.insert(url).second) {
        return;
    }
    thumbnailRequests.push_back(url);
    thumbnailCv.notify_one();
}

void ServerBrowserView::processThumbnailUploads() {
    std::deque<ThumbnailPayload> results;
    {
        std::lock_guard<std::mutex> lock(thumbnailMutex);
        results.swap(thumbnailResults);
    }

    for (auto &payload : results) {
        {
            std::lock_guard<std::mutex> lock(thumbnailMutex);
            thumbnailInFlight.erase(payload.url);
        }

        auto &entry = thumbnailCache[payload.url];
        entry.loading = false;

        if (payload.failed || payload.pixels.empty() || payload.width <= 0 || payload.height <= 0) {
            entry.failed = true;
            continue;
        }

        GLuint textureId = 0;
        glGenTextures(1, &textureId);
        glBindTexture(GL_TEXTURE_2D, textureId);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, payload.width, payload.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, payload.pixels.data());
        glBindTexture(GL_TEXTURE_2D, 0);

        entry.textureId = textureId;
        entry.width = payload.width;
        entry.height = payload.height;
        entry.failed = false;
    }
}

void ServerBrowserView::thumbnailWorkerProc() {
    auto fetchBytes = [](const std::string &url, std::vector<unsigned char> &body) {
        if (!bz::net::EnsureCurlGlobalInit()) {
            return false;
        }

        CURL *curlHandle = curl_easy_init();
        if (!curlHandle) {
            return false;
        }

        curl_easy_setopt(curlHandle, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curlHandle, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curlHandle, CURLOPT_TIMEOUT, 5L);
        curl_easy_setopt(curlHandle, CURLOPT_WRITEFUNCTION, CurlWriteToVector);
        curl_easy_setopt(curlHandle, CURLOPT_WRITEDATA, &body);

        CURLcode result = curl_easy_perform(curlHandle);
        long status = 0;
        if (result == CURLE_OK) {
            curl_easy_getinfo(curlHandle, CURLINFO_RESPONSE_CODE, &status);
        }
        curl_easy_cleanup(curlHandle);

        if (result != CURLE_OK || status < 200 || status >= 300 || body.empty()) {
            return false;
        }

        return true;
    };

    while (true) {
        std::string url;
        {
            std::unique_lock<std::mutex> lock(thumbnailMutex);
            thumbnailCv.wait(lock, [&]() { return thumbnailWorkerStop || !thumbnailRequests.empty(); });
            if (thumbnailWorkerStop) {
                return;
            }
            url = std::move(thumbnailRequests.front());
            thumbnailRequests.pop_front();
        }

        ThumbnailPayload payload;
        payload.url = url;
        payload.failed = true;

        std::vector<unsigned char> body;
        if (fetchBytes(url, body)) {
            int width = 0;
            int height = 0;
            int channels = 0;
            stbi_uc *pixels = stbi_load_from_memory(body.data(), static_cast<int>(body.size()), &width, &height, &channels, 4);
            if (pixels && width > 0 && height > 0) {
                payload.width = width;
                payload.height = height;
                payload.failed = false;
                payload.pixels.assign(pixels, pixels + (width * height * 4));
            }
            if (pixels) {
                stbi_image_free(pixels);
            }
        }

        {
            std::lock_guard<std::mutex> lock(thumbnailMutex);
            thumbnailResults.push_back(std::move(payload));
        }
    }
}

} // namespace gui
