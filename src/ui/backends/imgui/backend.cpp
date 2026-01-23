#include "ui/backends/imgui/backend.hpp"

#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include "common/data_path_resolver.hpp"
#include "common/config_helpers.hpp"
#include "spdlog/spdlog.h"
#include <algorithm>

namespace ui {

ImGuiBackend::ImGuiBackend(GLFWwindow *window) : window(window) {
    // Initialize ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    // Disable ImGui ini persistence to avoid restoring stray debug/demo windows.
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    
    // Set ImGui style
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer bindings
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    // Load fonts
    io.Fonts->AddFontDefault();

    const auto bigFontPath = bz::data::ResolveConfiguredAsset("guiBigFont");
    const std::string bigFontPathStr = bigFontPath.string();
    bigFont = io.Fonts->AddFontFromFileTTF(
        bigFontPathStr.c_str(),
        100.0f   // font size in pixels
    );

    if (!bigFont) {
        spdlog::warn("UiSystem: Failed to load font at {}", bigFontPathStr);
    }

    consoleView.initializeFonts(io);

	showFPS = bz::data::ReadBoolConfig({"debug.ShowFPS"}, false);
	hud.setShowFps(showFPS);

    io.Fonts->Build();
}

ImGuiBackend::~ImGuiBackend() {
    // Cleanup ImGui
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void ImGuiBackend::update() {
    if (consoleView.consumeFontReloadRequest()) {
        reloadFonts();
    }
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGuiIO& io = ImGui::GetIO();
    io.FontGlobalScale = 1.0f; // keep text size constant across window resizes
    ImGui::NewFrame();

    if (consoleView.isVisible()) {
        consoleView.draw(io);
    } else {
        hud.setShowFps(showFPS);
        hud.draw(io, bigFont);
    }

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void ImGuiBackend::reloadFonts() {
    ImGuiIO &io = ImGui::GetIO();
    io.Fonts->Clear();
    io.Fonts->AddFontDefault();

    const auto bigFontPath = bz::data::ResolveConfiguredAsset("guiBigFont");
    const std::string bigFontPathStr = bigFontPath.string();
    bigFont = io.Fonts->AddFontFromFileTTF(
        bigFontPathStr.c_str(),
        100.0f
    );

    if (!bigFont) {
        spdlog::warn("UiSystem: Failed to load font at {}", bigFontPathStr);
    }

    consoleView.initializeFonts(io);
    io.Fonts->Build();
    ImGui_ImplOpenGL3_DestroyFontsTexture();
    ImGui_ImplOpenGL3_CreateFontsTexture();
}

void ImGuiBackend::setScoreboardEntries(const std::vector<ScoreboardEntry> &entries) {
    hud.setScoreboardEntries(entries);
}

void ImGuiBackend::setSpawnHint(const std::string &hint) {
    hud.setSpawnHint(hint);
}

void ImGuiBackend::setRadarTextureId(unsigned int textureId) {
    hud.setRadarTextureId(textureId);
}

void ImGuiBackend::addConsoleLine(const std::string &playerName, const std::string &line) {
    hud.addConsoleLine(playerName, line);
}

std::string ImGuiBackend::getChatInputBuffer() const {
    return hud.getChatInputBuffer();
}

void ImGuiBackend::clearChatInputBuffer() {
    hud.clearChatInputBuffer();
}

void ImGuiBackend::focusChatInput() {
    hud.focusChatInput();
}

bool ImGuiBackend::getChatInputFocus() const {
    return hud.getChatInputFocus();
}

void ImGuiBackend::displayDeathScreen(bool show) {
    hud.displayDeathScreen(show);
}

ConsoleInterface &ImGuiBackend::console() {
    return consoleView;
}

const ConsoleInterface &ImGuiBackend::console() const {
    return consoleView;
}

} // namespace ui
