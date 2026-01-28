#include <cstdlib>
#include <filesystem>
#include <initializer_list>
#include <string>
#include "spdlog/spdlog.h"
#include "engine/client_engine.hpp"
#if defined(BZ3_RENDER_BACKEND_BGFX)
#include "engine/graphics/backends/bgfx/backend.hpp"
#endif
#if defined(BZ3_RENDER_BACKEND_FILAMENT)
#include "engine/graphics/backends/filament/backend.hpp"
#endif
#include "client/game.hpp"
#include "client/client_cli_options.hpp"
#include "client/config_client.hpp"
#include "client/server/community_browser_controller.hpp"
#include "client/server/server_connector.hpp"
#include "game/common/data_path_spec.hpp"
#include "common/data_dir_override.hpp"
#include "common/data_path_resolver.hpp"
#include "common/config_helpers.hpp"
#include "common/i18n.hpp"
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

void SetEnvOverride(const char *name, const std::string &value) {
    if (!name || value.empty()) {
        return;
    }
#if defined(_WIN32)
    _putenv_s(name, value.c_str());
#else
    setenv(name, value.c_str(), 1);
#endif
    spdlog::info("Env override set: {}={}", name, value);
}

int main(int argc, char *argv[]) {
    ConfigureLogging(spdlog::level::info, false);

    game_common::ConfigureDataPathSpec();

    const bz::data::DataDirOverrideResult dataDirResult = bz::data::ApplyDataDirOverrideFromArgs(argc, argv);

    const auto clientUserConfigPathFs = dataDirResult.userConfigPath;
    const std::vector<bz::data::ConfigLayerSpec> clientConfigSpecs = {
        {"common/config.json", "data/common/config.json", spdlog::level::err, true},
        {"client/config.json", "data/client/config.json", spdlog::level::err, true},
        {clientUserConfigPathFs, "user config", spdlog::level::debug, false}
    };
    bz::data::InitializeConfigCache(clientConfigSpecs);
    bz::i18n::Get().loadFromConfig();

    const uint16_t configWidth = bz::data::ReadUInt16Config({"graphics.resolution.Width"}, 1280);
    const uint16_t configHeight = bz::data::ReadUInt16Config({"graphics.resolution.Height"}, 720);
    const bool fullscreenEnabled = bz::data::ReadBoolConfig({"graphics.Fullscreen"}, false);
    const bool vsyncEnabled = bz::data::ReadBoolConfig({"graphics.VSync"}, true);

    const ClientCLIOptions cliOptions = ParseClientCLIOptions(argc, argv);
    if (cliOptions.languageExplicit && !cliOptions.language.empty()) {
        bz::i18n::Get().loadLanguage(cliOptions.language);
    }
    if (cliOptions.themeExplicit && !cliOptions.theme.empty()) {
        SetEnvOverride("BZ3_BGFX_THEME", cliOptions.theme);
    }
#if defined(BZ3_RENDER_BACKEND_BGFX)
    if (cliOptions.forceWaylandVulkan) {
        graphics_backend::SetBgfxRendererPreference(graphics_backend::BgfxRendererPreference::Vulkan);
    } else if (cliOptions.rendererExplicit) {
        if (cliOptions.renderer == "vulkan") {
            graphics_backend::SetBgfxRendererPreference(graphics_backend::BgfxRendererPreference::Vulkan);
        }
    }
#endif
#if defined(BZ3_RENDER_BACKEND_FILAMENT)
    graphics_backend::SetFilamentBackendPreference(graphics_backend::FilamentBackendPreference::Vulkan);
#endif
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
    windowConfig.preferredVideoDriver = bz::data::ReadStringConfig({"platform.SdlVideoDriver"}, "");
    if (cliOptions.forceWaylandVulkan) {
        windowConfig.preferredVideoDriver = "wayland";
    } else if (cliOptions.videoDriverExplicit) {
        windowConfig.preferredVideoDriver = (cliOptions.videoDriver == "auto") ? std::string() : cliOptions.videoDriver;
    }
    auto window = platform::CreateWindow(windowConfig);
    if (!window || !window->nativeHandle()) {
        spdlog::error("Window failed to create");
        return 1;
    }
#if defined(BZ3_RENDER_BACKEND_FILAMENT)
    if (cliOptions.forceWaylandVulkan || (cliOptions.rendererExplicit && cliOptions.renderer == "vulkan")) {
        const std::string driver = window->getVideoDriver();
        if (driver != "wayland") {
            spdlog::error("Filament Vulkan requires Wayland; SDL video driver is '{}'", driver.empty() ? "(null)" : driver);
            return 1;
        }
    }
#endif

    window->setVsync(vsyncEnabled);

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

    }

    return 0;
}
