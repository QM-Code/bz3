#pragma once

#include <cstring>
#include <string>

#include "ui/console/console_types.hpp"

namespace ui::status_banner {

struct StatusBanner {
    std::string text;
    MessageTone tone = MessageTone::Notice;
    bool visible = false;
};

struct StatusBannerStyle {
    const char *noticePrefix = "";
    const char *errorPrefix = "! ";
    const char *pendingPrefix = "... ";
    bool includePrefix = true;
};

inline StatusBanner MakeStatusBanner(const std::string &text, bool isError) {
    StatusBanner banner;
    banner.text = text;
    banner.visible = !text.empty();
    banner.tone = isError ? MessageTone::Error : MessageTone::Notice;
    return banner;
}

inline std::string FormatStatusText(const StatusBanner &banner,
                                    const StatusBannerStyle &style = StatusBannerStyle{}) {
    if (!banner.visible) {
        return {};
    }
    if (!style.includePrefix) {
        return banner.text;
    }
    const char *prefix = "";
    switch (banner.tone) {
        case MessageTone::Error:
            prefix = style.errorPrefix;
            break;
        case MessageTone::Pending:
            prefix = style.pendingPrefix;
            break;
        case MessageTone::Notice:
        default:
            prefix = style.noticePrefix;
            break;
    }
    if (!prefix || prefix[0] == '\0') {
        return banner.text;
    }
    std::string out;
    out.reserve(std::strlen(prefix) + banner.text.size());
    out.append(prefix);
    out.append(banner.text);
    return out;
}

} // namespace ui::status_banner
