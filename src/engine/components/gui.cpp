#include "engine/components/gui.hpp"
#include <GLFW/glfw3.h>
#include "common/data_path_resolver.hpp"
#include "common/config_helpers.hpp"
#include "spdlog/spdlog.h"
#include <algorithm>

GUI::GUI(GLFWwindow *window) {
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
        spdlog::warn("GUI: Failed to load font at {}", bigFontPathStr);
    }

    mainMenuView.initializeFonts(io);

	showFPS = bz::data::ReadBoolConfig({"debug.ShowFPS"}, false);

    io.Fonts->Build();
}

GUI::~GUI() {
    // Cleanup ImGui
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void GUI::update() {
    if (mainMenuView.consumeFontReloadRequest()) {
        reloadFonts();
    }
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGuiIO& io = ImGui::GetIO();
    io.FontGlobalScale = 1.0f; // keep text size constant across window resizes
    ImGui::NewFrame();

    if (mainMenuView.isVisible()) {
        mainMenuView.draw(io);
    } else {
        // Render HUD (scoreboard, console, crosshair)
        ImGui::SetNextWindowPos(ImVec2(20, 20));
        ImGui::SetNextWindowSize(ImVec2(500, 200));
        ImGui::SetNextWindowBgAlpha(0.0f);

        ImGui::Begin("TopLeftText", nullptr,
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoSavedSettings
        );

        for (const auto& entry : scoreboardEntries) {
            const char *prefix = "  ";
            if (entry.communityAdmin) {
                prefix = "@ ";
            } else if (entry.localAdmin) {
                prefix = "* ";
            } else if (entry.registeredUser) {
                prefix = "+ ";
            }
            ImGui::Text("%s%s  (%d)", prefix, entry.name.c_str(), entry.score);
        }
        ImGui::End();

        drawConsolePanel();

        if (drawDeathScreenFlag) {
            drawDeathScreen();
        }

        const float boxSize = 50.0f;
        ImVec2 center(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f);

        ImVec2 p0(
            center.x - boxSize * 0.5f,
            center.y - boxSize * 0.5f
        );

        ImVec2 p1(
            center.x + boxSize * 0.5f,
            center.y + boxSize * 0.5f
        );

        ImGui::GetForegroundDrawList()->AddRect(
            p0,
            p1,
            IM_COL32(200, 200, 200, 180),
            0.0f,
            0,
            1.0f
        );

        if (showFPS) {
            const float margin = 16.0f;
            ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - margin, margin), ImGuiCond_Always, ImVec2(1.0f, 0.0f));
            ImGui::SetNextWindowBgAlpha(0.35f);
            ImGui::Begin("##FPSOverlay", nullptr,
                ImGuiWindowFlags_NoTitleBar |
                ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_NoScrollbar |
                ImGuiWindowFlags_NoSavedSettings |
                ImGuiWindowFlags_AlwaysAutoResize);
            ImGui::Text("FPS: %.1f", io.Framerate);
            ImGui::End();
        }
    }

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void GUI::reloadFonts() {
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
        spdlog::warn("GUI: Failed to load font at {}", bigFontPathStr);
    }

    mainMenuView.initializeFonts(io);
    io.Fonts->Build();
    ImGui_ImplOpenGL3_DestroyFontsTexture();
    ImGui_ImplOpenGL3_CreateFontsTexture();
}

void GUI::drawDeathScreen() {
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 screenCenter = ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f);

    const char* text = spawnHint.c_str();

    const float fontScale = 0.55f; // smaller so long hints stay on one line
    ImFont* font = bigFont ? bigFont : ImGui::GetFont();
    const float drawSize = font ? font->FontSize * fontScale : ImGui::GetFontSize() * fontScale;
    ImVec2 textSize;
    if (font) {
        textSize = font->CalcTextSizeA(drawSize, FLT_MAX, 0.0f, text);
    } else {
        const ImVec2 raw = ImGui::CalcTextSize(text);
        textSize = ImVec2(raw.x * fontScale, raw.y * fontScale);
    }

    const ImVec2 textPos(
        screenCenter.x - textSize.x * 0.5f,
        screenCenter.y - textSize.y * 0.5f
    );

    auto* drawList = ImGui::GetForegroundDrawList();
    const ImU32 shadowColor = IM_COL32(0, 0, 0, 180);
    const ImU32 textColor = IM_COL32(255, 255, 255, 255);
    const ImVec2 shadowOffset(2.0f, 2.0f);

    const ImVec2 shadowPos(textPos.x + shadowOffset.x, textPos.y + shadowOffset.y);
    drawList->AddText(font, drawSize, shadowPos, shadowColor, text);
    drawList->AddText(font, drawSize, textPos, textColor, text);
}

void GUI::setScoreboardEntries(const std::vector<ScoreboardEntry> &entries) {
    scoreboardEntries = entries;
}

void GUI::setSpawnHint(const std::string &hint) {
    spawnHint = hint;
}

void GUI::drawConsolePanel() {
    ImGuiIO& io = ImGui::GetIO();
    ImGuiViewport* vp = ImGui::GetMainViewport();

    const float margin = 12.0f;
    const float panelHeight = 260.0f;     // tweak
    const float inputHeight = 34.0f;      // tweak

    ImVec2 vpPos  = vp->Pos;        // usually (0,0)
    ImVec2 vpSize = vp->Size;

    float radarSize = std::clamp(vpSize.y * 0.35f, 240.0f, 460.0f);
    radarSize = std::min(radarSize, vpSize.y - 2.0f * margin);
    radarSize = std::min(radarSize, vpSize.x - 2.0f * margin);

    const ImVec2 radarPos = ImVec2(vpPos.x + margin, vpPos.y + vpSize.y - margin - radarSize);
    const ImVec2 radarWindowSize = ImVec2(radarSize, radarSize);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));

    ImGui::SetNextWindowPos(radarPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(radarWindowSize, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.35f);
    ImGui::Begin("##RadarPanel", nullptr,
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoSavedSettings
    );

    if (radarTextureId != 0) {
        ImGui::SetCursorPos(ImVec2(0.0f, 0.0f));
        const ImVec2 imageSize = ImGui::GetContentRegionAvail();
        ImTextureID tex = (ImTextureID)(intptr_t)radarTextureId;
        // Flip vertically (OpenGL textures are typically bottom-left origin)
        ImGui::Image(tex, imageSize, ImVec2(0, 1), ImVec2(1, 0));
    } else {
        ImGui::TextUnformatted("Radar");
        ImGui::Separator();
        ImGui::TextDisabled("(waiting for radar texture)");
    }
    ImGui::End();

    ImGui::PopStyleVar(4);

    const float consoleLeft = vpPos.x + margin + radarSize + margin;
    const float consoleWidth = std::max(50.0f, vpSize.x - (radarSize + 3.0f * margin));
    ImVec2 pos  = ImVec2(consoleLeft, vpPos.y + vpSize.y - margin - panelHeight);
    ImVec2 size = ImVec2(consoleWidth, panelHeight);

    ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(size, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.70f);

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings;

    

    ImGui::Begin("##BottomConsole", nullptr, flags);

    // Split area: scroll region above, input hint below
    const float footer = inputHeight;
    ImGui::BeginChild("##ConsoleScroll",
                      ImVec2(0, -footer),
                      true,
                      ImGuiWindowFlags_HorizontalScrollbar);

    // Auto-scroll logic: keep at bottom unless user scrolls up.
    // Call this bool member variable in your GUI class (persist between frames):
    // bool consoleAutoScroll = true;
    // Or use a static for quick testing:
    static bool autoScroll = true;

    // If user scrolls up, disable auto-scroll until they return to bottom
    const float scrollY = ImGui::GetScrollY();
    const float scrollMaxY = ImGui::GetScrollMaxY();
    const float atBottomEpsilon = 2.0f;
    if (scrollMaxY > 0.0f) {
        if (scrollY < scrollMaxY - atBottomEpsilon) {
            autoScroll = false;
        } else {
            autoScroll = true;
        }
    }

    // Render lines
    // Replace consoleLines with your storage (old messages, console logs, etc.)
    for (const std::string& line : consoleLines) {
        ImGui::TextUnformatted(line.c_str());
    }

    // If auto-scroll, pin to bottom AFTER adding items
    if (autoScroll) {
        ImGui::SetScrollHereY(1.0f);
    }

    ImGui::EndChild();

    // Footer hint / fake input box
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::InvisibleButton("##focus_sink", ImVec2(1, 1));

    if (chatFocus) { 
        ImGui::SetKeyboardFocusHere();
    } else {
        ImGui::SetKeyboardFocusHere(-1);
    }

    ImGui::PushItemWidth(-1);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.75f);
    bool submitted = ImGui::InputTextWithHint(
        "##ChatHint",
        "press T to type",
        chatInputBuffer.data(),
        chatInputBuffer.size(),
        ImGuiInputTextFlags_EnterReturnsTrue
    );

    // Escape handling (same frame)
    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        chatInputBuffer.fill(0);
        submittedInputBuffer.clear();
        chatFocus = false;
        spdlog::trace("GUI::drawConsolePanel: Chat input cancelled with Escape.");
    }
    
    ImGui::PopStyleVar();
    ImGui::PopItemWidth();

    ImGui::End();

    if (submitted) {
        submittedInputBuffer = std::string(chatInputBuffer.data());
        chatInputBuffer.fill(0); 
        chatFocus = false;
        spdlog::trace("GUI::drawConsolePanel: Submitted chat input: {}", submittedInputBuffer);
    }
}

void GUI::addConsoleLine(const std::string &playerName, const std::string &line) {
    std::string displayName = playerName;
    if (!displayName.empty() && displayName.front() != '[') {
        displayName = "[" + displayName + "]";
    }
    std::string fullLine = displayName.empty() ? line : (displayName + " " + line);
    consoleLines.push_back(fullLine);
}

std::string GUI::getChatInputBuffer() const {
    return submittedInputBuffer;
}

void GUI::clearChatInputBuffer() {
    submittedInputBuffer.clear();
}

void GUI::focusChatInput() {
    chatFocus = true;
}

bool GUI::getChatInputFocus() const {
    return chatFocus;
}

void GUI::displayDeathScreen(bool show) {
    drawDeathScreenFlag = show;
}

gui::MainMenuView &GUI::mainMenu() {
    return mainMenuView;
}

const gui::MainMenuView &GUI::mainMenu() const {
    return mainMenuView;
}
