#include "ui/frontends/imgui/console/console.hpp"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "common/json.hpp"

#include "common/data_path_resolver.hpp"

namespace {

ImVec4 readColorArray(const bz::json::Value &value, const ImVec4 &fallback) {
    if (!value.is_array()) {
        return fallback;
    }
    if (value.size() < 3 || value.size() > 4) {
        return fallback;
    }
    auto readComponent = [&](std::size_t index, float defaultValue) -> float {
        if (index >= value.size() || !value[index].is_number()) {
            return defaultValue;
        }
        return static_cast<float>(value[index].get<double>());
    };
    ImVec4 color = fallback;
    color.x = readComponent(0, color.x);
    color.y = readComponent(1, color.y);
    color.z = readComponent(2, color.z);
    if (value.size() >= 4) {
        color.w = readComponent(3, color.w);
    }
    return color;
}

} // namespace

namespace ui {

void ConsoleView::drawThemesPanel(const MessageColors &colors) {
    ensureThemesLoaded();

    ImGui::PushStyleColor(ImGuiCol_Text, headingColor);
    ImGui::TextUnformatted("Themes");
    ImGui::PopStyleColor();
    ImGui::Spacing();

    const char *currentThemeLabel = themeOptions.empty()
        ? "Default"
        : themeOptions[static_cast<std::size_t>(selectedThemeIndex)].c_str();
    if (ImGui::BeginCombo("Theme", currentThemeLabel)) {
        for (int i = 0; i < static_cast<int>(themeOptions.size()); ++i) {
            const bool selected = (i == selectedThemeIndex);
            if (ImGui::Selectable(themeOptions[i].c_str(), selected)) {
                selectedThemeIndex = i;
                applyThemeSelection(themeOptions[i]);
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    ImGui::InputText("Theme name", themeNameBuffer.data(), themeNameBuffer.size());

    auto editFont = [&](const char *label, ThemeFontConfig &fontConfig) -> bool {
        bool changed = false;
        ImGui::Separator();
        ImGui::TextUnformatted(label);
        if (!fontConfig.font.empty()) {
            ImGui::SameLine();
            ImGui::TextDisabled("(%s)", fontConfig.font.c_str());
        }
        changed |= ImGui::SliderFloat((std::string(label) + " size").c_str(), &fontConfig.size, 12.0f, 48.0f, "%.1f");
        changed |= ImGui::ColorEdit4((std::string(label) + " color").c_str(), &fontConfig.color.x);
        return changed;
    };

    bool changed = false;
    changed |= editFont("Regular", currentTheme.regular);
    changed |= editFont("Title", currentTheme.title);
    changed |= editFont("Heading", currentTheme.heading);
    changed |= editFont("Button", currentTheme.button);
    if (changed) {
        themeDirty = true;
        applyThemeToView(currentTheme);
        useThemeOverrides = true;
        fontReloadRequested = true;
    }

    ImGui::Spacing();
    bool saveClicked = false;
    bool resetClicked = false;
    const bool hasButtonFont = (buttonFont != nullptr);
    if (hasButtonFont) {
        ImGui::PushFont(buttonFont);
    }
    ImGui::PushStyleColor(ImGuiCol_Text, buttonColor);
    if (ImGui::Button("Save Theme")) {
        saveClicked = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset to Default")) {
        resetClicked = true;
    }
    ImGui::PopStyleColor();
    if (hasButtonFont) {
        ImGui::PopFont();
    }

    if (saveClicked) {
        std::string themeName = std::string(themeNameBuffer.data());
        if (themeName.empty()) {
            themeStatusText = "Theme name is required.";
            themeStatusIsError = true;
        } else {
            currentTheme.name = themeName;
            themePresets[themeName] = currentTheme;
            if (std::find(themeOptions.begin(), themeOptions.end(), themeName) == themeOptions.end()) {
                themeOptions.push_back(themeName);
            }
            applyThemeSelection(themeName);
            themeStatusText = "Theme saved.";
            themeStatusIsError = false;
            themeDirty = false;
            useThemeOverrides = false;
        }
    }

    if (resetClicked) {
        resetToDefaultTheme();
        themeStatusText = "Theme reset to default.";
        themeStatusIsError = false;
        useThemeOverrides = false;
    }

    if (!themeStatusText.empty()) {
        ImVec4 statusColor = themeStatusIsError ? colors.error : colors.notice;
        ImGui::TextColored(statusColor, "%s", themeStatusText.c_str());
    }
}

void ConsoleView::applyThemeToView(const ThemeConfig &theme) {
    regularColor = theme.regular.color;
    titleColor = theme.title.color;
    headingColor = theme.heading.color;
    buttonColor = theme.button.color;
}

bool ConsoleView::loadUserConfig(bz::json::Value &out) const {
    const std::filesystem::path path = userConfigPath.empty()
        ? bz::data::EnsureUserConfigFile("config.json")
        : std::filesystem::path(userConfigPath);
    if (auto user = bz::data::LoadJsonFile(path, "user config", spdlog::level::debug)) {
        if (!user->is_object()) {
            out = bz::json::Object();
            return false;
        }
        out = *user;
        return true;
    }
    out = bz::json::Object();
    return true;
}

bool ConsoleView::saveUserConfig(const bz::json::Value &userConfig, std::string &error) const {
    const std::filesystem::path path = userConfigPath.empty()
        ? bz::data::EnsureUserConfigFile("config.json")
        : std::filesystem::path(userConfigPath);
    std::error_code ec;
    const auto parentDir = path.parent_path();
    if (!parentDir.empty()) {
        std::filesystem::create_directories(parentDir, ec);
        if (ec) {
            error = "Failed to create config directory.";
            return false;
        }
    }

    std::ofstream file(path, std::ios::trunc);
    if (!file.is_open()) {
        error = "Failed to open user config for writing.";
        return false;
    }

    try {
        file << userConfig.dump(4) << '\n';
    } catch (const std::exception &ex) {
        error = ex.what();
        return false;
    }
    return true;
}

void ConsoleView::setNestedConfig(bz::json::Value &root,
                                           std::initializer_list<const char*> path,
                                           bz::json::Value value) const {
    bz::json::Value *node = &root;
    for (auto it = path.begin(); it != path.end(); ++it) {
        const bool isLast = std::next(it) == path.end();
        const std::string key(*it);
        if (isLast) {
            (*node)[key] = std::move(value);
        } else {
            if (!node->contains(key) || !(*node)[key].is_object()) {
                (*node)[key] = bz::json::Object();
            }
            node = &(*node)[key];
        }
    }
}

void ConsoleView::setNestedConfig(bz::json::Value &root,
                                           const std::vector<std::string> &path,
                                           bz::json::Value value) const {
    bz::json::Value *node = &root;
    for (std::size_t i = 0; i < path.size(); ++i) {
        const std::string &key = path[i];
        const bool isLast = (i + 1 == path.size());
        if (isLast) {
            (*node)[key] = std::move(value);
        } else {
            if (!node->contains(key) || !(*node)[key].is_object()) {
                (*node)[key] = bz::json::Object();
            }
            node = &(*node)[key];
        }
    }
}

void ConsoleView::eraseNestedConfig(bz::json::Value &root,
                                             std::initializer_list<const char*> path) const {
    std::vector<std::pair<bz::json::Value*, std::string>> chain;
    bz::json::Value *node = &root;
    for (const char *segment : path) {
        if (!node->is_object()) {
            return;
        }
        auto it = node->find(segment);
        if (it == node->end()) {
            return;
        }
        chain.emplace_back(node, segment);
        node = &(*it);
    }

    if (chain.empty()) {
        return;
    }
    chain.back().first->erase(chain.back().second);
    for (auto it = chain.rbegin() + 1; it != chain.rend(); ++it) {
        if (it->first->is_object() && it->first->empty()) {
            auto parent = it + 1;
            parent->first->erase(it->second);
        } else {
            break;
        }
    }
}

bz::json::Value ConsoleView::themeToJson(const ThemeConfig &theme) const {
    auto encodeFont = [](const ThemeFontConfig &font) {
        return bz::json::Value{
            {"Font", font.font},
            {"Size", font.size},
            {"Color", bz::json::Array({font.color.x, font.color.y, font.color.z, font.color.w})}
        };
    };
    bz::json::Value console = bz::json::Object();
    console["Regular"] = encodeFont(theme.regular);
    console["Title"] = encodeFont(theme.title);
    console["Heading"] = encodeFont(theme.heading);
    console["Button"] = encodeFont(theme.button);
    return console;
}

ConsoleView::ThemeConfig ConsoleView::themeFromJson(const bz::json::Value &themeJson,
                                                                      const ThemeConfig &fallback) const {
    const bz::json::Value *consoleNode = nullptr;
    if (themeJson.is_object()) {
        if (auto assetsIt = themeJson.find("assets"); assetsIt != themeJson.end() && assetsIt->is_object()) {
            const auto &assetsObj = *assetsIt;
            if (auto hudIt = assetsObj.find("hud"); hudIt != assetsObj.end() && hudIt->is_object()) {
                const auto &hudObj = *hudIt;
                if (auto fontsIt = hudObj.find("fonts"); fontsIt != hudObj.end() && fontsIt->is_object()) {
                    const auto &fontsObj = *fontsIt;
                    if (auto consoleIt = fontsObj.find("console"); consoleIt != fontsObj.end() && consoleIt->is_object()) {
                        consoleNode = &(*consoleIt);
                    }
                }
            }
        }
        if (!consoleNode) {
            if (auto fontsIt = themeJson.find("fonts"); fontsIt != themeJson.end() && fontsIt->is_object()) {
                const auto &fontsObj = *fontsIt;
                if (auto consoleIt = fontsObj.find("console"); consoleIt != fontsObj.end() && consoleIt->is_object()) {
                    consoleNode = &(*consoleIt);
                }
            }
        }
        if (!consoleNode) {
            if (themeJson.contains("Regular") || themeJson.contains("Title") ||
                themeJson.contains("Heading") || themeJson.contains("Button")) {
                consoleNode = &themeJson;
            }
        }
    }

    if (!consoleNode || !consoleNode->is_object()) {
        return fallback;
    }

    auto readFont = [&](const char *key, const ThemeFontConfig &fallbackFont) -> ThemeFontConfig {
        ThemeFontConfig font = fallbackFont;
        auto it = consoleNode->find(key);
        if (it == consoleNode->end() || !it->is_object()) {
            return font;
        }
        const auto &node = *it;
        if (auto fontIt = node.find("Font"); fontIt != node.end() && fontIt->is_string()) {
            font.font = fontIt->get<std::string>();
        }
        if (auto sizeIt = node.find("Size"); sizeIt != node.end()) {
            if (sizeIt->is_number()) {
                font.size = static_cast<float>(sizeIt->get<double>());
            } else if (sizeIt->is_string()) {
                try {
                    font.size = std::stof(sizeIt->get<std::string>());
                } catch (...) {
                }
            }
        }
        if (auto colorIt = node.find("Color"); colorIt != node.end()) {
            font.color = readColorArray(*colorIt, font.color);
        }
        return font;
    };

    ThemeConfig theme = fallback;
    theme.regular = readFont("Regular", fallback.regular);
    theme.title = readFont("Title", fallback.title);
    theme.heading = readFont("Heading", fallback.heading);
    theme.button = readFont("Button", fallback.button);
    return theme;
}

void ConsoleView::applyThemeSelection(const std::string &name) {
    if (name == "Default") {
        resetToDefaultTheme();
        return;
    }

    ThemeConfig selected = defaultTheme;
    if (name == "Custom" && customTheme) {
        selected = *customTheme;
    } else {
        auto it = themePresets.find(name);
        if (it != themePresets.end()) {
            selected = it->second;
        }
    }

    bz::json::Value userConfig;
    if (!loadUserConfig(userConfig)) {
        themeStatusText = "Failed to load user config.";
        themeStatusIsError = true;
        return;
    }

    bz::json::Value consoleJson = themeToJson(selected);
    setNestedConfig(userConfig, {"assets", "hud", "fonts", "console"}, consoleJson);

    if (!name.empty()) {
        setNestedConfig(userConfig, {"gui", "themes", "active"}, name);
    }
    if (name != "Custom" && name != "Default") {
        setNestedConfig(
            userConfig,
            std::vector<std::string>{"gui", "themes", "presets", name},
            bz::json::Value{{"fonts", {{"console", consoleJson}}}});
    }

    std::string error;
    if (!saveUserConfig(userConfig, error)) {
        themeStatusText = error.empty() ? "Failed to save theme." : error;
        themeStatusIsError = true;
        return;
    }

    currentTheme = selected;
    currentTheme.name = name;
    applyThemeToView(currentTheme);
    fontReloadRequested = true;
    themeDirty = false;
    useThemeOverrides = false;

    for (std::size_t i = 0; i < themeOptions.size(); ++i) {
        if (themeOptions[i] == name) {
            selectedThemeIndex = static_cast<int>(i);
            break;
        }
    }
}

void ConsoleView::resetToDefaultTheme() {
    bz::json::Value userConfig;
    if (!loadUserConfig(userConfig)) {
        themeStatusText = "Failed to load user config.";
        themeStatusIsError = true;
        return;
    }

    eraseNestedConfig(userConfig, {"assets", "hud", "fonts", "console"});
    eraseNestedConfig(userConfig, {"gui", "themes", "active"});

    std::string error;
    if (!saveUserConfig(userConfig, error)) {
        themeStatusText = error.empty() ? "Failed to save theme." : error;
        themeStatusIsError = true;
        return;
    }

    currentTheme = defaultTheme;
    applyThemeToView(currentTheme);
    selectedThemeIndex = 0;
    themeNameBuffer[0] = '\0';
    fontReloadRequested = true;
    themeDirty = false;
    useThemeOverrides = false;
}


void ConsoleView::ensureThemesLoaded() {
    if (themesLoaded) {
        return;
    }
    themesLoaded = true;
    themeOptions.clear();
    themePresets.clear();
    customTheme.reset();
    themeStatusText.clear();
    themeStatusIsError = false;

    const ImVec4 defaultTextColor = ImGui::GetStyle().Colors[ImGuiCol_Text];
    ThemeConfig fallback;
    fallback.name = "Default";
    fallback.regular = ThemeFontConfig{"", 20.0f, defaultTextColor};
    fallback.title = ThemeFontConfig{"", 28.0f, defaultTextColor};
    fallback.heading = ThemeFontConfig{"", 28.0f, defaultTextColor};
    fallback.button = ThemeFontConfig{"", 18.0f, defaultTextColor};

    defaultTheme = fallback;
    const auto defaultsPath = bz::data::Resolve("client/config.json");
    if (auto defaults = bz::data::LoadJsonFile(defaultsPath, "client defaults", spdlog::level::debug)) {
        if (defaults->is_object()) {
            defaultTheme = themeFromJson(*defaults, defaultTheme);
        }
    }
    defaultTheme.name = "Default";

    bz::json::Value userConfig;
    if (loadUserConfig(userConfig)) {
        if (auto guiIt = userConfig.find("gui"); guiIt != userConfig.end() && guiIt->is_object()) {
            auto &guiObj = *guiIt;
            if (auto themesIt = guiObj.find("themes"); themesIt != guiObj.end() && themesIt->is_object()) {
                auto &themesObj = *themesIt;
                if (auto presetsIt = themesObj.find("presets"); presetsIt != themesObj.end() && presetsIt->is_object()) {
                    for (const auto &item : presetsIt->items()) {
                        if (!item.value().is_object()) {
                            continue;
                        }
                        ThemeConfig parsed = themeFromJson(item.value(), defaultTheme);
                        parsed.name = item.key();
                        themePresets[item.key()] = parsed;
                    }
                }
            }
        }

        const bz::json::Value *consoleNode = nullptr;
        if (auto assetsIt = userConfig.find("assets"); assetsIt != userConfig.end() && assetsIt->is_object()) {
            const auto &assetsObj = *assetsIt;
            if (auto hudIt = assetsObj.find("hud"); hudIt != assetsObj.end() && hudIt->is_object()) {
                const auto &hudObj = *hudIt;
                if (auto fontsIt = hudObj.find("fonts"); fontsIt != hudObj.end() && fontsIt->is_object()) {
                    const auto &fontsObj = *fontsIt;
                    if (auto consoleIt = fontsObj.find("console"); consoleIt != fontsObj.end() && consoleIt->is_object()) {
                        consoleNode = &(*consoleIt);
                    }
                }
            }
        }

        if (consoleNode) {
            ThemeConfig parsed = themeFromJson(*consoleNode, defaultTheme);
            parsed.name = "Custom";
            customTheme = parsed;
        }
    }

    themeOptions.push_back("Default");
    std::vector<std::string> presetNames;
    presetNames.reserve(themePresets.size());
    for (const auto &entry : themePresets) {
        presetNames.push_back(entry.first);
    }
    std::sort(presetNames.begin(), presetNames.end());
    for (const auto &name : presetNames) {
        themeOptions.push_back(name);
    }
    if (customTheme) {
        themeOptions.push_back("Custom");
    }

    std::string activeName;
    if (loadUserConfig(userConfig)) {
        if (auto guiIt = userConfig.find("gui"); guiIt != userConfig.end() && guiIt->is_object()) {
            const auto &guiObj = *guiIt;
            if (auto themesIt = guiObj.find("themes"); themesIt != guiObj.end() && themesIt->is_object()) {
                const auto &themesObj = *themesIt;
                if (auto activeIt = themesObj.find("active"); activeIt != themesObj.end() && activeIt->is_string()) {
                    activeName = activeIt->get<std::string>();
                }
            }
        }
    }

    selectedThemeIndex = 0;
    currentTheme = defaultTheme;
    if (!activeName.empty()) {
        auto presetIt = themePresets.find(activeName);
        if (presetIt != themePresets.end()) {
            currentTheme = presetIt->second;
            for (std::size_t i = 0; i < themeOptions.size(); ++i) {
                if (themeOptions[i] == activeName) {
                    selectedThemeIndex = static_cast<int>(i);
                    break;
                }
            }
        } else if (activeName == "Custom" && customTheme) {
            currentTheme = *customTheme;
            for (std::size_t i = 0; i < themeOptions.size(); ++i) {
                if (themeOptions[i] == "Custom") {
                    selectedThemeIndex = static_cast<int>(i);
                    break;
                }
            }
        }
    } else if (customTheme) {
        currentTheme = *customTheme;
        for (std::size_t i = 0; i < themeOptions.size(); ++i) {
            if (themeOptions[i] == "Custom") {
                selectedThemeIndex = static_cast<int>(i);
                break;
            }
        }
    }

    applyThemeToView(currentTheme);
    if (!currentTheme.name.empty() && currentTheme.name != "Default" && currentTheme.name != "Custom") {
        std::snprintf(themeNameBuffer.data(), themeNameBuffer.size(), "%s", currentTheme.name.c_str());
    } else {
        themeNameBuffer[0] = '\0';
    }
    themeDirty = false;
}


} // namespace ui
