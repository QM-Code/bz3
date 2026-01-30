#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace ui::fonts {

enum class Script {
    Default,
    Cyrillic,
    Arabic,
    Devanagari,
    CjkJp,
    CjkKr,
    CjkSc
};

struct ConsoleFontSelection {
    std::string regularFontKey;
    std::vector<std::string> fallbackKeys;
    Script script = Script::Default;
};

struct ConsoleFontAssets {
    ConsoleFontSelection selection;
    std::string titleKey;
    std::string headingKey;
    std::string buttonKey;
    std::string emojiKey;
    std::vector<std::filesystem::path> extraPaths;
};

inline ConsoleFontSelection SelectConsoleFonts(const std::string &language,
                                               bool includeLatinFallback) {
    ConsoleFontSelection selection;
    selection.regularFontKey = "hud.fonts.console.Regular.Font";
    auto addFallback = [&](const char *key) {
        for (const auto &existing : selection.fallbackKeys) {
            if (existing == key) {
                return;
            }
        }
        selection.fallbackKeys.emplace_back(key);
    };
    if (includeLatinFallback) {
        addFallback("hud.fonts.console.FallbackLatin.Font");
    }

    if (language == "ru") {
        selection.regularFontKey = "hud.fonts.console.FallbackLatin.Font";
        selection.script = Script::Cyrillic;
        addFallback("hud.fonts.console.FallbackLatin.Font");
    } else if (language == "ar") {
        selection.regularFontKey = "hud.fonts.console.FallbackArabic.Font";
        selection.script = Script::Arabic;
        addFallback("hud.fonts.console.FallbackArabic.Font");
    } else if (language == "hi") {
        selection.regularFontKey = "hud.fonts.console.FallbackDevanagari.Font";
        selection.script = Script::Devanagari;
        addFallback("hud.fonts.console.FallbackDevanagari.Font");
    } else if (language == "jp") {
        selection.regularFontKey = "hud.fonts.console.FallbackCJK_JP.Font";
        selection.script = Script::CjkJp;
        addFallback("hud.fonts.console.FallbackCJK_JP.Font");
    } else if (language == "ko") {
        selection.regularFontKey = "hud.fonts.console.FallbackCJK_KR.Font";
        selection.script = Script::CjkKr;
        addFallback("hud.fonts.console.FallbackCJK_KR.Font");
    } else if (language == "zh") {
        selection.regularFontKey = "hud.fonts.console.FallbackCJK_SC.Font";
        selection.script = Script::CjkSc;
        addFallback("hud.fonts.console.FallbackCJK_SC.Font");
    }

    return selection;
}

ConsoleFontAssets GetConsoleFontAssets(const std::string &language, bool includeLatinFallback);

} // namespace ui::fonts
