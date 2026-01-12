#include "engine/components/gui/server_browser.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <imgui.h>
#include <imgui_internal.h>
#include <spdlog/spdlog.h>
#include <string>

#include "common/data_path_resolver.hpp"

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

    auto formatListLabel = [](const ServerListOption &option) {
        if (!option.name.empty()) {
            return option.name;
        }
        if (!option.url.empty()) {
            return option.url;
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

    ImVec4 scanColor = scanning ?
        ImVec4(0.60f, 0.80f, 0.40f, 1.0f) :
        ImVec4(0.70f, 0.70f, 0.70f, 1.0f);
    const char *scanLabel = scanning ? "Scanning..." : "Idle";
    const float refreshButtonWidth = ImGui::CalcTextSize("Refresh").x + style.FramePadding.x * 2.0f;
    const float statusTextWidth = ImGui::CalcTextSize(scanLabel).x;

    const ImGuiTableFlags tableFlags =
        ImGuiTableFlags_Resizable |
        ImGuiTableFlags_RowBg |
        ImGuiTableFlags_BordersOuter |
        ImGuiTableFlags_ScrollY;

    const float tableHeight = 260.0f;

    if (ImGui::BeginTable("##ServerBrowserPresets", 1, tableFlags, ImVec2(-1.0f, tableHeight))) {
        ImGui::TableSetupColumn("##ServerListColumn", ImGuiTableColumnFlags_None, 1.0f);

        ImGui::TableNextRow(ImGuiTableRowFlags_Headers);
        ImGui::TableSetColumnIndex(0);

        const char *serversHeadingLabel = "Servers";
        ImVec2 headingTextSize;
        if (hasHeadingFont) {
            ImGui::PushFont(headingFont);
            headingTextSize = ImGui::CalcTextSize(serversHeadingLabel);
            ImGui::PopFont();
        } else {
            headingTextSize = ImGui::CalcTextSize(serversHeadingLabel);
        }

        const float headerStartX = ImGui::GetCursorPosX();
        const float headerStartY = ImGui::GetCursorPosY();
        const float headerWidth = ImGui::GetContentRegionAvail().x;
        float buttonX = headerStartX + headerWidth - refreshButtonWidth;
        float statusX = buttonX - style.ItemSpacing.x - statusTextWidth;
        const float minStatusX = headerStartX + headingTextSize.x + style.ItemSpacing.x;
        if (statusX < minStatusX) {
            statusX = minStatusX;
            buttonX = statusX + style.ItemSpacing.x + refreshButtonWidth;
        }

        const float maxButtonX = headerStartX + headerWidth - refreshButtonWidth;
        if (buttonX > maxButtonX) {
            buttonX = maxButtonX;
            statusX = std::max(headerStartX + headingTextSize.x + style.ItemSpacing.x,
                buttonX - style.ItemSpacing.x - statusTextWidth);
        }

        ImGui::SetCursorPos(ImVec2(headerStartX, headerStartY));
        if (hasHeadingFont) {
            ImGui::PushFont(headingFont);
        }
        ImGui::TextUnformatted(serversHeadingLabel);
        if (hasHeadingFont) {
            ImGui::PopFont();
        }
        float lineBottom = ImGui::GetCursorPosY();

        ImGui::SetCursorPos(ImVec2(statusX, headerStartY));
        ImGui::TextColored(scanColor, "%s", scanLabel);
        lineBottom = std::max(lineBottom, ImGui::GetCursorPosY());

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
            ImGui::TextDisabled("No saved servers yet.");
        } else {
            for (int i = 0; i < static_cast<int>(entries.size()); ++i) {
                const auto &entry = entries[i];
                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                bool selected = (selectedIndex == i);
                std::string selectableLabel = entry.label + "##server_row_" + std::to_string(i);
                if (ImGui::Selectable(selectableLabel.c_str(), selected,
                        ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick)) {
                    selectedIndex = i;
                    if (ImGui::IsMouseDoubleClicked(0)) {
                        pendingSelection = ServerBrowserSelection{ entry.host, entry.port, true };
                        statusText.clear();
                        statusIsError = false;
                    }
                }
            }
        }

        ImGui::EndTable();
    }

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
            statusText = "Enter a server address before joining.";
            statusIsError = true;
        } else {
            auto colonPos = addressValue.find_last_of(':');
            if (colonPos == std::string::npos) {
                const std::string exampleAddress = "localhost:" + configuredServerPortLabel();
                statusText = "Use the format host:port (example: " + exampleAddress + ").";
                statusIsError = true;
            } else {
                std::string hostPart = trimCopy(addressValue.substr(0, colonPos));
                std::string portPart = trimCopy(addressValue.substr(colonPos + 1));

                if (hostPart.empty()) {
                    statusText = "Hostname cannot be empty.";
                    statusIsError = true;
                } else if (portPart.empty()) {
                    statusText = "Port cannot be empty.";
                    statusIsError = true;
                } else {
                    try {
                        int portValue = std::stoi(portPart);
                        if (portValue < 1 || portValue > 65535) {
                            statusText = "Ports must be between 1 and 65535.";
                            statusIsError = true;
                        } else {
                            pendingSelection = ServerBrowserSelection{
                                hostPart,
                                static_cast<uint16_t>(portValue),
                                false
                            };
                            statusText.clear();
                            statusIsError = false;
                        }
                    } catch (...) {
                        statusText = "Port must be a valid number.";
                        statusIsError = true;
                    }
                }
            }
        }
    }

    if (!statusText.empty()) {
        ImGui::Spacing();
        ImVec4 color = statusIsError ?
            ImVec4(0.93f, 0.36f, 0.36f, 1.0f) :
            ImVec4(0.60f, 0.80f, 0.40f, 1.0f);
        ImGui::TextColored(color, "%s", statusText.c_str());
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::Text("Add server list");
    ImGui::InputText("List URL", listUrlBuffer.data(), listUrlBuffer.size());

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
            listStatusText = "Enter a URL before saving.";
            listStatusIsError = true;
        } else {
            listStatusText.clear();
            listStatusIsError = false;
            pendingNewList = ServerListOption{ std::string{}, urlValue };
        }
    }

    if (!listStatusText.empty()) {
        ImGui::Spacing();
        ImVec4 listColor = listStatusIsError ?
            ImVec4(0.93f, 0.36f, 0.36f, 1.0f) :
            ImVec4(0.60f, 0.80f, 0.40f, 1.0f);
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
            pendingSelection = ServerBrowserSelection{ entry.host, entry.port, true };
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
    pendingListSelection.reset();
    pendingNewList.reset();
    listStatusText.clear();
    listStatusIsError = false;
    resetBuffers(defaultHost, defaultPort);
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
    pendingSelection.reset();
    pendingListSelection.reset();
    pendingNewList.reset();
    refreshRequested = false;
    scanning = false;
    listStatusText.clear();
    listStatusIsError = false;
}

bool ServerBrowserView::isVisible() const {
    return visible;
}

void ServerBrowserView::setStatus(const std::string &text, bool isErrorMessage) {
    statusText = text;
    statusIsError = isErrorMessage;
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

} // namespace gui
