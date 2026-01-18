#include "engine/components/gui/main_menu.hpp"

#include <algorithm>
#include <cctype>
#include <limits>
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

void MainMenuView::drawCommunityPanel(const MessageColors &messageColors) {
    const bool hasHeadingFont = (headingFont != nullptr);
    const bool hasButtonFont = (buttonFont != nullptr);
    bool joinFromIdentity = false;

    ImVec2 contentAvail = ImGui::GetContentRegionAvail();
    const ImGuiStyle &style = ImGui::GetStyle();
    const float minDetailWidth = 300.0f;
    const float minListWidth = 280.0f;
    float maxListWidth = std::max(minListWidth, contentAvail.x - minDetailWidth - style.ItemSpacing.x);
    float listPanelWidth = std::max(320.0f, contentAvail.x * 0.5f);
    listPanelWidth = std::clamp(listPanelWidth, minListWidth, maxListWidth);

    ImGui::BeginChild("CommunityBrowserListPane", ImVec2(listPanelWidth, 0), false);

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

    if (ImGui::BeginTable("##CommunityBrowserPresets", 2, tableFlags, ImVec2(-1.0f, tableHeight))) {
        ImGui::TableSetupColumn("##ServerListColumn", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn("##PlayerCountColumn", ImGuiTableColumnFlags_WidthFixed, playerColumnWidth);

        ImGui::TableNextRow(ImGuiTableRowFlags_Headers);

        const char *serversHeadingLabel = "Servers";

        ImGui::TableSetColumnIndex(0);
        if (hasHeadingFont) {
            ImGui::PushFont(headingFont);
        }
        ImGui::PushStyleColor(ImGuiCol_Text, headingColor);
        ImGui::TextUnformatted(serversHeadingLabel);
        ImGui::PopStyleColor();
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
        ImGui::PushStyleColor(ImGuiCol_Text, buttonColor);
        if (ImGui::Button("Refresh")) {
            refreshRequested = true;
        }
        ImGui::PopStyleColor();
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
                ImGui::PushStyleColor(ImGuiCol_Text, statusColor);
                ImGui::TextWrapped("%s", communityStatusText.c_str());
                ImGui::PopStyleColor();
            } else if (!listStatusText.empty()) {
                ImVec4 listColor = listStatusIsError ? messageColors.error : messageColors.action;
                ImGui::PushStyleColor(ImGuiCol_Text, listColor);
                ImGui::TextWrapped("%s", listStatusText.c_str());
                ImGui::PopStyleColor();
            } else {
                ImGui::TextDisabled("No servers available.");
            }
        } else {
            for (int i = 0; i < static_cast<int>(entries.size()); ++i) {
                const auto &entry = entries[i];
                bool selected = (i == selectedIndex);
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);

                std::string label = entry.label;
                if (label.empty()) {
                    label = entry.host;
                }

                if (ImGui::Selectable(label.c_str(), selected,
                                      ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick)) {
                    selectedIndex = i;
                    if (ImGui::IsMouseDoubleClicked(0)) {
                        pendingSelection = CommunityBrowserSelection{
                            entry.host,
                            entry.port,
                            false,
                            entry.sourceHost,
                            entry.worldName
                        };
                    }
                }
                ImGui::TableSetColumnIndex(1);
                if (entry.activePlayers >= 0) {
                    if (entry.maxPlayers >= 0) {
                        std::string activeText = std::to_string(entry.activePlayers);
                        std::string maxText = std::to_string(entry.maxPlayers);
                        ImVec2 activeSize = ImGui::CalcTextSize(activeText.c_str());
                        ImVec2 maxSize = ImGui::CalcTextSize(maxText.c_str());
                        float totalWidth = activeSize.x + maxSize.x + ImGui::CalcTextSize(" / ").x;
                        float columnWidth = ImGui::GetColumnWidth();
                        float startX = ImGui::GetCursorPosX() + std::max(0.0f, columnWidth - totalWidth);
                        ImGui::SetCursorPosX(startX);
                        ImGui::TextUnformatted(activeText.c_str());
                        ImGui::SameLine(0.0f, 0.0f);
                        ImGui::TextUnformatted(" / ");
                        ImGui::SameLine(0.0f, 0.0f);
                        ImGui::TextUnformatted(maxText.c_str());
                    } else {
                        ImGui::Text("%d", entry.activePlayers);
                    }
                } else {
                    ImGui::TextUnformatted("-");
                }
            }
        }
        ImGui::EndTable();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    refreshCommunityCredentials();

    bool usernameChanged = false;
    bool passwordChanged = false;

    const bool isLanCommunity =
        listSelectedIndex >= 0 &&
        listSelectedIndex < static_cast<int>(listOptions.size()) &&
        listOptions[listSelectedIndex].name == "Local Area Network";

    const float joinInlineWidth = ImGui::CalcTextSize("Join").x + style.FramePadding.x * 2.0f;
    const float labelSpacing = style.ItemSpacing.x * 2.0f;
    const float inputWidth = 150.0f;
    float rowWidth = ImGui::GetContentRegionAvail().x - joinInlineWidth - style.ItemSpacing.x;
    float contentWidth = inputWidth + ImGui::CalcTextSize("Username").x + style.ItemInnerSpacing.x;
    if (!isLanCommunity) {
        contentWidth += labelSpacing;
        contentWidth += ImGui::CalcTextSize("Password").x + style.ItemInnerSpacing.x + inputWidth;
    }

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Username");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(inputWidth);
    const bool usernameEdited = ImGui::InputText(
        "##Username",
        usernameBuffer.data(),
        usernameBuffer.size(),
        ImGuiInputTextFlags_EnterReturnsTrue);
    joinFromIdentity |= usernameEdited;
    usernameChanged |= usernameEdited;
    if (usernameEdited) {
        storedPasswordHash.clear();
        passwordChanged = true;
    }

    if (!isLanCommunity) {
        ImGui::SameLine(0.0f, labelSpacing);
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Password");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(inputWidth);
        const char *passwordHint = storedPasswordHash.empty() ? "" : "stored";
        const bool passwordEdited = ImGui::InputTextWithHint(
            "##Password",
            passwordHint,
            passwordBuffer.data(),
            passwordBuffer.size(),
            ImGuiInputTextFlags_Password | ImGuiInputTextFlags_EnterReturnsTrue);
        joinFromIdentity |= passwordEdited;
        if (passwordEdited) {
            storedPasswordHash.clear();
            passwordChanged = true;
        }
    }

    if (rowWidth > contentWidth) {
        ImGui::SameLine(0.0f, rowWidth - contentWidth);
    } else {
        ImGui::SameLine();
    }
    if (hasButtonFont) {
        ImGui::PushFont(buttonFont);
    }
    ImGui::PushStyleColor(ImGuiCol_Text, buttonColor);
    if (ImGui::Button("Join")) {
        joinFromIdentity = true;
    }
    ImGui::PopStyleColor();
    if (hasButtonFont) {
        ImGui::PopFont();
    }

    if (usernameChanged || passwordChanged) {
        persistCommunityCredentials(passwordChanged);
    }

    if (!statusText.empty()) {
        ImGui::Spacing();
        ImVec4 color = statusIsError ? messageColors.error : messageColors.action;
        ImGui::TextColored(color, "%s", statusText.c_str());
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::PushStyleColor(ImGuiCol_Text, headingColor);
    ImGui::Text("Custom server");
    ImGui::PopStyleColor();
    ImGui::InputText("Address (host:port)", addressBuffer.data(), addressBuffer.size());

    bool joinCustomClicked = false;
    if (hasButtonFont) {
        ImGui::PushFont(buttonFont);
    }
    ImGui::PushStyleColor(ImGuiCol_Text, buttonColor);
    if (ImGui::Button("Join Custom")) {
        joinCustomClicked = true;
    }
    ImGui::PopStyleColor();
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
                const std::string hostValue = addressValue.substr(0, colonPos);
                const std::string portValue = addressValue.substr(colonPos + 1);
                try {
                    int port = std::stoi(portValue);
                    if (port <= 0 || port > std::numeric_limits<uint16_t>::max()) {
                        customStatusText = "Port must be a valid number.";
                        customStatusIsError = true;
                    } else {
                        pendingSelection = CommunityBrowserSelection{
                            hostValue,
                            static_cast<uint16_t>(port),
                            true,
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

    if (!customStatusText.empty()) {
        ImGui::Spacing();
        ImVec4 color = customStatusIsError ? messageColors.error : messageColors.action;
        ImGui::TextColored(color, "%s", customStatusText.c_str());
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::PushStyleColor(ImGuiCol_Text, headingColor);
    ImGui::Text("Add server list");
    ImGui::PopStyleColor();
    ImGui::InputText("Community host", listUrlBuffer.data(), listUrlBuffer.size());

    bool saveListClicked = false;
    if (hasButtonFont) {
        ImGui::PushFont(buttonFont);
    }
    ImGui::PushStyleColor(ImGuiCol_Text, buttonColor);
    if (ImGui::Button("Save Server List")) {
        saveListClicked = true;
    }
    ImGui::PopStyleColor();
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

    ImGui::BeginChild("CommunityBrowserDetailsPane", ImVec2(0, 0), true);
    if (hasHeadingFont) {
        ImGui::PushFont(headingFont);
    }
    ImGui::PushStyleColor(ImGuiCol_Text, headingColor);
    ImGui::TextUnformatted("Server Details");
    ImGui::PopStyleColor();
    if (hasHeadingFont) {
        ImGui::PopFont();
    }
    ImGui::SameLine();
    const float joinButtonWidth = ImGui::CalcTextSize("Join").x + style.FramePadding.x * 2.0f;
    const float joinButtonOffset = std::max(0.0f, ImGui::GetContentRegionAvail().x - joinButtonWidth);
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + joinButtonOffset);
    bool joinSelectedClicked = joinFromIdentity;
    if (hasButtonFont) {
        ImGui::PushFont(buttonFont);
    }
    ImGui::PushStyleColor(ImGuiCol_Text, buttonColor);
    if (ImGui::Button("Join")) {
        joinSelectedClicked = true;
    }
    ImGui::PopStyleColor();
    if (hasButtonFont) {
        ImGui::PopFont();
    }
    if (joinSelectedClicked) {
        if (selectedIndex >= 0 && selectedIndex < static_cast<int>(entries.size())) {
            const auto &entry = entries[selectedIndex];
            pendingSelection = CommunityBrowserSelection{
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

    const CommunityBrowserEntry *selectedEntry = nullptr;
    if (selectedIndex >= 0 && selectedIndex < static_cast<int>(entries.size())) {
        selectedEntry = &entries[selectedIndex];
    }

    if (!selectedEntry) {
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextDisabled("Select a server to view details.");
    } else {
        ImGui::Spacing();
        ImGui::Separator();
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
        ImGui::PushStyleColor(ImGuiCol_Text, headingColor);
        ImGui::TextUnformatted("Description");
        ImGui::PopStyleColor();
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
                    ImGui::PushStyleColor(ImGuiCol_Text, headingColor);
                    ImGui::TextUnformatted("Screenshot");
                    ImGui::PopStyleColor();

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
        ImGui::PushStyleColor(ImGuiCol_Text, headingColor);
        ImGui::TextUnformatted("Plugins");
        ImGui::PopStyleColor();
        if (!selectedEntry->flags.empty()) {
            for (const auto &flag : selectedEntry->flags) {
                ImGui::BulletText("%s", flag.c_str());
            }
        } else {
            ImGui::TextDisabled("No plugins reported.");
        }
    }

    ImGui::EndChild();
}

} // namespace gui
