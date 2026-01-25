#include <filesystem>
#include <initializer_list>
#include <string>
#include <glad/glad.h>
#include "spdlog/spdlog.h"
#include "engine/client_engine.hpp"
#include "client/game.hpp"
#include "client/client_cli_options.hpp"
#include "client/config_client.hpp"
#include "client/server/community_browser_controller.hpp"
#include "client/server/server_connector.hpp"
#include "common/data_dir_override.hpp"
#include "common/data_path_resolver.hpp"
#include "common/config_helpers.hpp"
#include "platform/window.hpp"

TimeUtils::time lastFrameTime;
constexpr TimeUtils::duration MIN_DELTA_TIME = 1.0f / 120.0f;

namespace {
struct FullscreenState {
    bool active = false;
};
}

spdlog::level::level_enum ParseLogLevel(const std::string &level) {
    if (level == "trace") {
        return spdlog::level::trace;
    }
    if (level == "debug") {
        return spdlog::level::debug;
    }
    if (level == "info") {
        return spdlog::level::info;
    }
    if (level == "warn") {
        return spdlog::level::warn;
    }
    if (level == "err") {
        return spdlog::level::err;
    }
    if (level == "critical") {
        return spdlog::level::critical;
    }
    if (level == "off") {
        return spdlog::level::off;
    }
    return spdlog::level::info;
}

void ConfigureLogging(spdlog::level::level_enum level, bool includeTimestamp) {
    if (includeTimestamp) {
        spdlog::set_pattern("%Y-%m-%d %H:%M:%S.%e [%^%l%$] %v");
    } else {
        spdlog::set_pattern("[%^%l%$] %v");
    }
    spdlog::set_level(level);
}

int main(int argc, char *argv[]) {
    ConfigureLogging(spdlog::level::info, false);

    const bz::data::DataDirOverrideResult dataDirResult = bz::data::ApplyDataDirOverrideFromArgs(argc, argv);

    const auto clientUserConfigPathFs = dataDirResult.userConfigPath;
    const std::vector<bz::data::ConfigLayerSpec> clientConfigSpecs = {
        {"common/config.json", "data/common/config.json", spdlog::level::err, true},
        {"client/config.json", "data/client/config.json", spdlog::level::err, true},
        {clientUserConfigPathFs, "user config", spdlog::level::debug, false}
    };
    bz::data::InitializeConfigCache(clientConfigSpecs);

    const uint16_t configWidth = bz::data::ReadUInt16Config({"graphics.resolution.Width"}, 1280);
    const uint16_t configHeight = bz::data::ReadUInt16Config({"graphics.resolution.Height"}, 720);
    const bool fullscreenEnabled = bz::data::ReadBoolConfig({"graphics.Fullscreen"}, false);
    const bool vsyncEnabled = bz::data::ReadBoolConfig({"graphics.VSync"}, true);

    const ClientCLIOptions cliOptions = ParseClientCLIOptions(argc, argv);
    const spdlog::level::level_enum logLevel = cliOptions.logLevelExplicit
        ? ParseLogLevel(cliOptions.logLevel)
        : (cliOptions.verbose ? spdlog::level::trace : spdlog::level::info);
    ConfigureLogging(logLevel, cliOptions.timestampLogging);

    const std::string clientUserConfigPath = clientUserConfigPathFs.string();
    ClientConfig clientConfig = ClientConfig::Load("");

    const std::string initialWorldDir = (cliOptions.worldExplicit && !cliOptions.worldDir.empty())
        ? cliOptions.worldDir
        : bz::data::Resolve("client-test").string();

    platform::WindowConfig windowConfig;
    windowConfig.width = configWidth;
    windowConfig.height = configHeight;
    windowConfig.title = "BZFlag v3";
    windowConfig.glMajor = 3;
    windowConfig.glMinor = 3;
    windowConfig.samples = 4;

    auto window = platform::CreateWindow(windowConfig);
    if (!window || !window->nativeHandle()) {
        spdlog::error("Window failed to create");
        return 1;
    }

    if (gladLoadGL() == 0) {
        spdlog::error("Failed to load OpenGL functions");
        return 1;
    }

    glEnable(GL_MULTISAMPLE);
    window->setVsync(vsyncEnabled);

    spdlog::info("GL_VENDOR   = {}", (const char*)glGetString(GL_VENDOR));
    spdlog::info("GL_RENDERER = {}", (const char*)glGetString(GL_RENDERER));
    spdlog::info("GL_VERSION  = {}", (const char*)glGetString(GL_VERSION));

    int sb = 0, s = 0;
    glGetIntegerv(GL_SAMPLE_BUFFERS, &sb);
    glGetIntegerv(GL_SAMPLES, &s);
    spdlog::info("GL_SAMPLE_BUFFERS={}, GL_SAMPLES={}", sb, s);

    ClientEngine engine(*window);
    spdlog::trace("ClientEngine initialized successfully");

    if (fullscreenEnabled) {
        window->setFullscreen(true);
    }

    std::unique_ptr<Game> game;
    ServerConnector serverConnector(engine, cliOptions.playerName, initialWorldDir, game);
    CommunityBrowserController communityBrowser(
        engine,
        clientConfig,
        clientUserConfigPath,
        serverConnector);

    if (cliOptions.addrExplicit) {
        serverConnector.connect(cliOptions.connectAddr, cliOptions.connectPort, cliOptions.playerName, false, false, false);
    }

    lastFrameTime = TimeUtils::GetCurrentTime();

    spdlog::trace("Starting main loop");

    while (!window->shouldClose()) {
        TimeUtils::time currTime = TimeUtils::GetCurrentTime();
        TimeUtils::duration deltaTime = TimeUtils::GetElapsedTime(lastFrameTime, currTime);

        if (deltaTime < MIN_DELTA_TIME) {
            TimeUtils::sleep(MIN_DELTA_TIME - deltaTime);
            continue;
        }

        lastFrameTime = currTime;

        engine.earlyUpdate(deltaTime);

        static bool prevGraveDown = false;
        const bool graveDown = window->isKeyDown(platform::Key::GraveAccent);
        if (graveDown && !prevGraveDown) {
            if (game) {
                auto &console = engine.ui->console();
                if (console.isVisible()) {
                    console.hide();
                } else {
                    console.show({});
                }
            }
        }
        prevGraveDown = graveDown;

        if (engine.ui->console().consumeQuitRequest()) {
            if (game) {
                engine.network->disconnect("Disconnected from server.");
            }
        }

        if (auto disconnectEvent = engine.network->consumeDisconnectEvent()) {
            if (game) {
                game.reset();
            }
            communityBrowser.handleDisconnected(disconnectEvent->reason);
        }

        if (engine.ui->console().isVisible()) {
            communityBrowser.update();
        } else if (game) {
            game->earlyUpdate(deltaTime);
            game->lateUpdate(deltaTime);
        }

        engine.step(deltaTime);
        engine.lateUpdate(deltaTime);

        window->swapBuffers();
    }

    return 0;
}
