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
    
    // Set ImGui style
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer bindings
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    // Load fonts
    ImGuiIO& io = ImGui::GetIO();
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

    serverBrowserView.initializeFonts(io);

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
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGuiIO& io = ImGui::GetIO();
    io.FontGlobalScale = 1.0f; // keep text size constant across window resizes
    ImGui::NewFrame();

    if (serverBrowserView.isVisible()) {
        serverBrowserView.draw(io);
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

        std::string players = "";
        for (const auto& name : scoreboardPlayerNames) {
            players += name + "\n";
        }

        ImGui::Text("%s", players.c_str());
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

void GUI::drawDeathScreen() {
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 screenCenter = ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f);

    ImGui::PushFont(bigFont);

    const char* text = "Press U to spawn";

    // Calculate text size
    ImVec2 textSize = ImGui::CalcTextSize(text);

    // Position cursor so text is centered
    ImGui::SetNextWindowPos(
        ImVec2(screenCenter.x - textSize.x * 0.5f,
            screenCenter.y - textSize.y * 0.5f),
        ImGuiCond_Always
    );

    // Invisible window
    ImGui::Begin(
        "##SpawnHint",
        nullptr,
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoInputs |
        ImGuiWindowFlags_NoBackground |
        ImGuiWindowFlags_AlwaysAutoResize
    );

    ImGui::TextUnformatted(text);

    ImGui::PopFont();

    ImGui::End();
}

void GUI::setScoreboardPlayerNames(const std::vector<std::string> &names) {
    scoreboardPlayerNames = names;
}

void GUI::drawConsolePanel() {
    ImGuiIO& io = ImGui::GetIO();
    ImGuiViewport* vp = ImGui::GetMainViewport();

    const float margin = 12.0f;
    const float panelHeight = 260.0f;     // tweak
    const float inputHeight = 34.0f;      // tweak

    ImVec2 vpPos  = vp->Pos;        // usually (0,0)
    ImVec2 vpSize = vp->Size;

    ImVec2 pos  = ImVec2(vpPos.x + margin, vpPos.y + vpSize.y - margin - panelHeight);
    ImVec2 size = ImVec2(vpSize.x - 2.0f * margin, panelHeight);

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

gui::ServerBrowserView &GUI::serverBrowser() {
    return serverBrowserView;
}

const gui::ServerBrowserView &GUI::serverBrowser() const {
    return serverBrowserView;
}
