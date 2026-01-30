#pragma once

#include <optional>
#include <string>

#include "common/json.hpp"

namespace ui {

class UiConfig {
public:
    static constexpr float kDefaultRenderBrightness = 1.0f;
    static constexpr float kDefaultRenderScale = 1.0f;
    static constexpr float kMinRenderScale = 0.5f;
    static constexpr float kMaxRenderScale = 1.0f;
    static constexpr bool kDefaultHudScoreboard = true;
    static constexpr bool kDefaultHudChat = true;
    static constexpr bool kDefaultHudRadar = true;
    static constexpr bool kDefaultHudFps = false;
    static constexpr bool kDefaultHudCrosshair = true;

    static float GetRenderBrightness();
    static bool SetRenderBrightness(float value);
    static bool EraseRenderBrightness();

    static std::optional<float> TryGetRenderScale();
    static bool SetRenderScale(float value);
    static bool EraseRenderScale();

    static std::string GetLanguage();
    static bool SetLanguage(const std::string &value);

    static const bz::json::Value *GetCommunityCredentials();
    static bool SetCommunityCredentials(const bz::json::Value &value);
    static bool EraseCommunityCredentials();

    static std::optional<bz::json::Value> GetKeybindings();
    static bool SetKeybindings(const bz::json::Value &value);
    static bool EraseKeybindings();

    static std::optional<bz::json::Value> GetControllerKeybindings();
    static bool SetControllerKeybindings(const bz::json::Value &value);
    static bool EraseControllerKeybindings();

    static bool GetHudScoreboard();
    static bool GetHudChat();
    static bool GetHudRadar();
    static bool GetHudFps();
    static bool GetHudCrosshair();

    static bool SetHudScoreboard(bool value);
    static bool SetHudChat(bool value);
    static bool SetHudRadar(bool value);
    static bool SetHudFps(bool value);
    static bool SetHudCrosshair(bool value);
};

} // namespace ui
