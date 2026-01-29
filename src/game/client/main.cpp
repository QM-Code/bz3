#include <cstdlib>
#include <filesystem>
#include <initializer_list>
#include <string>
#include <vector>
#include <cstring>
#include "spdlog/spdlog.h"
#include "engine/client_engine.hpp"
#if defined(BZ3_RENDER_BACKEND_BGFX)
#include "engine/graphics/backends/bgfx/backend.hpp"
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
#include "common/config_store.hpp"
#include "common/i18n.hpp"
#include "platform/window.hpp"

#if !defined(_WIN32)
#include <csignal>
#include <cerrno>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

TimeUtils::time lastFrameTime;
constexpr TimeUtils::duration MIN_DELTA_TIME = 1.0f / 120.0f;

namespace {
struct FullscreenState {
    bool active = false;
};

struct QuickStartServer {
#if !defined(_WIN32)
    pid_t pid = -1;
#endif
    ~QuickStartServer() {
#if !defined(_WIN32)
        if (pid > 0) {
            kill(pid, SIGTERM);
        }
#endif
    }
};

std::string findServerBinary() {
    std::error_code ec;
    const auto dataRoot = bz::data::DataRoot();
    const auto root = dataRoot.parent_path();

    auto isExecutable = [](const std::filesystem::path &path) {
        std::error_code localEc;
        if (!std::filesystem::exists(path, localEc)) {
            return false;
        }
#if defined(_WIN32)
        return true;
#else
        return ::access(path.c_str(), X_OK) == 0;
#endif
    };

    std::vector<std::filesystem::path> candidates = {
        root / "bz3-server",
        root / "build" / "bz3-server",
        root / "build" / "Debug" / "bz3-server",
        root / "build" / "Release" / "bz3-server"
    };

    for (const auto &candidate : candidates) {
        if (isExecutable(candidate)) {
            return candidate.string();
        }
    }

    std::vector<std::filesystem::path> searchDirs = {
        root,
        std::filesystem::current_path(ec)
    };

    for (const auto &dir : searchDirs) {
        if (dir.empty() || !std::filesystem::exists(dir, ec)) {
            continue;
        }
        std::error_code iterEc;
        for (auto it = std::filesystem::recursive_directory_iterator(dir, iterEc);
             it != std::filesystem::recursive_directory_iterator();
             ++it) {
            if (iterEc) {
                break;
            }
            if (it.depth() > 3) {
                it.disable_recursion_pending();
                continue;
            }
            const auto &path = it->path();
            if (path.filename() == "bz3-server" && isExecutable(path)) {
                return path.string();
            }
        }
    }

    return {};
}

bool launchQuickStartServer(const ClientCLIOptions &cliOptions, QuickStartServer &server) {
#if defined(_WIN32)
    (void)cliOptions;
    (void)server;
    spdlog::error("dev-quick-start: server auto-launch not supported on Windows yet.");
    return false;
#else
    const std::string serverBinary = findServerBinary();
    if (serverBinary.empty()) {
        spdlog::error("dev-quick-start: bz3-server binary not found.");
        return false;
    }

    pid_t pid = fork();
    if (pid < 0) {
        spdlog::error("dev-quick-start: failed to fork server process: {}", std::strerror(errno));
        return false;
    }

    if (pid == 0) {
        std::vector<std::string> args;
        args.push_back(serverBinary);
        args.push_back("-p");
        args.push_back(std::to_string(cliOptions.connectPort));
        args.push_back("-D");
        if (cliOptions.dataDirExplicit && !cliOptions.dataDir.empty()) {
            args.push_back("-d");
            args.push_back(cliOptions.dataDir);
        }

        std::vector<char*> argv;
        argv.reserve(args.size() + 1);
        for (auto &arg : args) {
            argv.push_back(arg.data());
        }
        argv.push_back(nullptr);

        execv(serverBinary.c_str(), argv.data());
        _exit(1);
    }

    server.pid = pid;
    spdlog::info("dev-quick-start: launched bz3-server (pid {}) on port {}", pid, cliOptions.connectPort);
    return true;
#endif
}
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
    const std::vector<bz::config::ConfigFileSpec> clientConfigSpecs = {
        {"common/config.json", "data/common/config.json", spdlog::level::err, true, true},
        {"client/config.json", "data/client/config.json", spdlog::level::err, true, true}
    };
    bz::config::ConfigStore::Initialize(clientConfigSpecs, clientUserConfigPathFs);
    bz::i18n::Get().loadFromConfig();

    const uint16_t configWidth = bz::config::ReadUInt16Config({"graphics.resolution.Width"}, 1280);
    const uint16_t configHeight = bz::config::ReadUInt16Config({"graphics.resolution.Height"}, 720);
    const bool fullscreenEnabled = bz::config::ReadBoolConfig({"graphics.Fullscreen"}, false);
    const bool vsyncEnabled = bz::config::ReadBoolConfig({"graphics.VSync"}, true);

    const ClientCLIOptions cliOptions = ParseClientCLIOptions(argc, argv);
    if (cliOptions.languageExplicit && !cliOptions.language.empty()) {
        bz::i18n::Get().loadLanguage(cliOptions.language);
    }
    if (cliOptions.themeExplicit && !cliOptions.theme.empty()) {
        SetEnvOverride("BZ3_BGFX_THEME", cliOptions.theme);
    }
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
    windowConfig.preferredVideoDriver = bz::config::ReadStringConfig({"platform.SdlVideoDriver"}, "");
    auto window = platform::CreateWindow(windowConfig);
    if (!window || !window->nativeHandle()) {
        spdlog::error("Window failed to create");
        return 1;
    }
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

    QuickStartServer quickStartServer;
    bool quickStartPending = false;
    int quickStartAttempts = 0;
    TimeUtils::time quickStartLastAttempt = TimeUtils::GetCurrentTime();
    const float quickStartRetryDelay = 0.5f;
    const int quickStartMaxAttempts = 20;

    if (cliOptions.devQuickStart) {
        engine.ui->console().show({});
        if (launchQuickStartServer(cliOptions, quickStartServer)) {
            quickStartPending = true;
            quickStartLastAttempt = TimeUtils::GetCurrentTime();
        }
    } else if (cliOptions.addrExplicit) {
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

        if (quickStartPending && !engine.network->isConnected()) {
            const TimeUtils::time now = TimeUtils::GetCurrentTime();
            if (TimeUtils::GetElapsedTime(quickStartLastAttempt, now) >= quickStartRetryDelay) {
                quickStartLastAttempt = now;
                ++quickStartAttempts;
                if (serverConnector.connect("localhost", cliOptions.connectPort, cliOptions.playerName, false, false, false)) {
                    quickStartPending = false;
                    if (cliOptions.devQuickStart) {
                        engine.ui->console().hide();
                    }
                } else if (quickStartAttempts >= quickStartMaxAttempts) {
                    spdlog::error("dev-quick-start: failed to connect after {} attempts.", quickStartAttempts);
                    quickStartPending = false;
                }
            }
        }

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
