#include "ui/fonts/console_fonts.hpp"

#include "common/config_store.hpp"
#include "common/data_path_resolver.hpp"

namespace ui::fonts {

ConsoleFontAssets GetConsoleFontAssets(const std::string &language, bool includeLatinFallback) {
    ConsoleFontAssets assets;
    assets.selection = SelectConsoleFonts(language, includeLatinFallback);
    assets.titleKey = "hud.fonts.console.Title.Font";
    assets.headingKey = "hud.fonts.console.Heading.Font";
    assets.buttonKey = "hud.fonts.console.Button.Font";
    assets.emojiKey = "hud.fonts.console.Emoji.Font";

    if (const auto *extras = karma::config::ConfigStore::Get("assets.hud.fonts.console.Extras")) {
        if (extras->is_array()) {
            for (const auto &entry : *extras) {
                if (!entry.is_string()) {
                    continue;
                }
                const std::string extra = entry.get<std::string>();
                std::filesystem::path extraPath;
                if (extra.rfind("client/", 0) == 0 || extra.rfind("common/", 0) == 0) {
                    extraPath = karma::data::Resolve(extra);
                } else {
                    extraPath = karma::data::Resolve(std::filesystem::path("client") / extra);
                }
                if (!extraPath.empty()) {
                    assets.extraPaths.emplace_back(extraPath);
                }
            }
        }
    }

    return assets;
}

} // namespace ui::fonts
