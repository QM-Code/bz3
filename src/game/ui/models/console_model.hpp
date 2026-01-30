#pragma once

#include <string>
#include <vector>

#include "ui/console/console_interface.hpp"
#include "ui/console/console_types.hpp"

namespace ui {

struct ConsoleCommunityModel {
    std::vector<CommunityBrowserEntry> entries;
    std::vector<ServerListOption> listOptions;
    int selectedIndex = -1;
    int listSelectedIndex = -1;
    int selectedServerIndex = -1;
    bool scanning = false;
    std::string statusText;
    bool statusIsError = false;
    std::string listStatusText;
    bool listStatusIsError = false;
    std::string detailsText;
    MessageTone statusTone = MessageTone::Notice;
    std::string communityStatusText;
    std::string communityLinkStatusText;
    bool communityLinkStatusIsError = false;
    std::string serverLinkStatusText;
    bool serverLinkStatusIsError = false;
    std::string serverDescriptionLoadingKey;
    bool serverDescriptionLoading = false;
    std::string serverDescriptionErrorKey;
    std::string serverDescriptionErrorText;
};

struct ConsoleModel {
    ConsoleCommunityModel community;
    ConsoleInterface::ConnectionState connectionState{};
};

} // namespace ui
