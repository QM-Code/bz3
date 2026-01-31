#include <cstdlib>
#include <filesystem>
#include <initializer_list>
#include <string>
#include <vector>
#include <cstring>
#include <utility>
#include "spdlog/spdlog.h"
#include "karma/app/engine_app.hpp"
#include "game/engine/client_engine.hpp"
#if defined(KARMA_RENDER_BACKEND_BGFX)
#include "karma/graphics/backends/bgfx/backend.hpp"
#endif
#include "client/game.hpp"
#include "client/client_cli_options.hpp"
#include "client/config_client.hpp"
#include "client/server/community_browser_controller.hpp"
#include "client/server/server_connector.hpp"
#include "game/common/data_path_spec.hpp"
#include "karma/common/data_dir_override.hpp"
#include "karma/common/data_path_resolver.hpp"
#include "karma/common/config_helpers.hpp"
#include "karma/common/config_store.hpp"
#include "karma/common/config_validation.hpp"
#include "karma/common/i18n.hpp"
#include "karma/platform/window.hpp"

#if !defined(_WIN32)
#include <csignal>
#include <cerrno>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

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
    const auto dataRoot = karma::data::DataRoot();
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

class ClientLoopAdapter final : public karma::app::GameInterface {
public:
    ClientLoopAdapter(platform::Window &window,
                      ClientEngine &engine,
                      ServerConnector &serverConnector,
                      CommunityBrowserController &communityBrowser,
                      ClientCLIOptions cliOptions,
                      std::unique_ptr<Game> &game)
        : window_(window),
          engine_(engine),
          serverConnector_(serverConnector),
          communityBrowser_(communityBrowser),
          cliOptions_(std::move(cliOptions)),
          game_(game) {}

    bool onInit(karma::app::EngineContext &context) override {
        engine_.render->setResourceRegistry(context.resources);
        if (cliOptions_.devQuickStart) {
            engine_.ui->console().show({});
            if (launchQuickStartServer(cliOptions_, quickStartServer_)) {
                quickStartPending_ = true;
                quickStartLastAttempt_ = TimeUtils::GetCurrentTime();
            }
        } else if (cliOptions_.addrExplicit) {
            engine_.setRoamingModeSession(false);
            serverConnector_.connect(cliOptions_.connectAddr,
                                     cliOptions_.connectPort,
                                     cliOptions_.playerName,
                                     false,
                                     false,
                                     false);
        }
        return true;
    }

    void onShutdown(karma::app::EngineContext &) override {}

    void onUpdate(karma::app::EngineContext &, float dt) override {
        lastDt_ = dt;
        if (dt < MIN_DELTA_TIME) {
            TimeUtils::sleep(MIN_DELTA_TIME - dt);
            return;
        }

        engine_.earlyUpdate(dt);

        if (quickStartPending_ && !engine_.network->isConnected()) {
            const TimeUtils::time now = TimeUtils::GetCurrentTime();
            if (TimeUtils::GetElapsedTime(quickStartLastAttempt_, now) >= quickStartRetryDelay_) {
                quickStartLastAttempt_ = now;
                ++quickStartAttempts_;
                engine_.setRoamingModeSession(false);
                if (serverConnector_.connect("localhost",
                                             cliOptions_.connectPort,
                                             cliOptions_.playerName,
                                             false,
                                             false,
                                             false)) {
                    quickStartPending_ = false;
                    if (cliOptions_.devQuickStart) {
                        engine_.ui->console().hide();
                    }
                } else if (quickStartAttempts_ >= quickStartMaxAttempts_) {
                    spdlog::error("dev-quick-start: failed to connect after {} attempts.", quickStartAttempts_);
                    quickStartPending_ = false;
                }
            }
        }

        const bool graveDown = window_.isKeyDown(platform::Key::GraveAccent);
        if (graveDown && !prevGraveDown_) {
            if (game_) {
                auto &console = engine_.ui->console();
                if (console.isVisible()) {
                    console.hide();
                } else {
                    console.show({});
                }
            }
        }
        prevGraveDown_ = graveDown;

        if (engine_.ui->console().consumeQuitRequest()) {
            if (game_) {
                engine_.network->disconnect("Disconnected from server.");
            }
        }

        if (auto disconnectEvent = engine_.network->consumeDisconnectEvent()) {
            if (game_) {
                game_.reset();
            }
            communityBrowser_.handleDisconnected(disconnectEvent->reason);
        }

        const bool consoleVisible = engine_.ui->console().isVisible();
        if (consoleVisible) {
            engine_.inputState = {};
        }
        if (!consoleVisible && engine_.getInputState().toggleFullscreen) {
            const bool wasFullscreen = window_.isFullscreen();
            spdlog::info("Fullscreen toggle requested (before={})", wasFullscreen);
            window_.setFullscreen(!wasFullscreen);
            const bool nowFullscreen = window_.isFullscreen();
            spdlog::info("Fullscreen toggle complete (after={})", nowFullscreen);
            if (nowFullscreen == wasFullscreen) {
                spdlog::warn("Fullscreen toggle had no effect");
            }
        }
        if (consoleVisible) {
            communityBrowser_.update();
        }
        if (game_) {
            game_->earlyUpdate(dt);
        }

        engine_.step(dt);
    }

    void onRender(karma::app::EngineContext &) override {
        if (game_) {
            game_->lateUpdate(lastDt_);
        }
        engine_.lateUpdate(lastDt_);
    }

    bool shouldQuit() const override { return window_.shouldClose(); }

private:
    platform::Window &window_;
    ClientEngine &engine_;
    ServerConnector &serverConnector_;
    CommunityBrowserController &communityBrowser_;
    ClientCLIOptions cliOptions_;
    std::unique_ptr<Game> &game_;
    QuickStartServer quickStartServer_;
    bool quickStartPending_ = false;
    int quickStartAttempts_ = 0;
    TimeUtils::time quickStartLastAttempt_ = TimeUtils::GetCurrentTime();
    bool prevGraveDown_ = false;
    const float quickStartRetryDelay_ = 0.5f;
    const int quickStartMaxAttempts_ = 20;
    float lastDt_ = 0.0f;
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

    const karma::data::DataDirOverrideResult dataDirResult = karma::data::ApplyDataDirOverrideFromArgs(argc, argv);

    const auto clientUserConfigPathFs = dataDirResult.userConfigPath;
    const std::vector<karma::config::ConfigFileSpec> clientConfigSpecs = {
        {"common/config.json", "data/common/config.json", spdlog::level::err, true, true},
        {"client/config.json", "data/client/config.json", spdlog::level::err, true, true}
    };
    karma::config::ConfigStore::Initialize(clientConfigSpecs, clientUserConfigPathFs);
    const ClientCLIOptions cliOptions = ParseClientCLIOptions(argc, argv);
    const auto configIssues = karma::config::ValidateRequiredKeys(karma::config::ClientRequiredKeys());
    if (!configIssues.empty()) {
        if (cliOptions.strictConfig) {
            spdlog::error("Config validation failed:");
        } else {
            spdlog::warn("Config validation reported issues:");
        }
        for (const auto &issue : configIssues) {
            if (cliOptions.strictConfig) {
                spdlog::error("  {}: {}", issue.path, issue.message);
            } else {
                spdlog::warn("  {}: {}", issue.path, issue.message);
            }
        }
        if (cliOptions.strictConfig) {
            return 1;
        }
    }
    karma::i18n::Get().loadFromConfig();

    const uint16_t configWidth = karma::config::ReadUInt16Config({"graphics.resolution.Width"}, 1280);
    const uint16_t configHeight = karma::config::ReadUInt16Config({"graphics.resolution.Height"}, 720);
    const bool fullscreenEnabled = karma::config::ReadBoolConfig({"graphics.Fullscreen"}, false);
    const bool vsyncEnabled = karma::config::ReadBoolConfig({"graphics.VSync"}, true);
    const std::string windowTitle = karma::config::ReadStringConfig({"platform.WindowTitle"}, "BZFlag v3");

    if (cliOptions.languageExplicit && !cliOptions.language.empty()) {
        karma::i18n::Get().loadLanguage(cliOptions.language);
    }
    if (cliOptions.themeExplicit && !cliOptions.theme.empty()) {
        SetEnvOverride("KARMA_BGFX_THEME", cliOptions.theme);
    }
    const spdlog::level::level_enum logLevel = cliOptions.logLevelExplicit
        ? ParseLogLevel(cliOptions.logLevel)
        : (cliOptions.verbose >= 2 ? spdlog::level::trace
           : cliOptions.verbose == 1 ? spdlog::level::debug
           : spdlog::level::info);
    ConfigureLogging(logLevel, cliOptions.timestampLogging);

    const std::string clientUserConfigPath = clientUserConfigPathFs.string();
    ClientConfig clientConfig = ClientConfig::Load("");

    const std::string initialWorldDir = (cliOptions.worldExplicit && !cliOptions.worldDir.empty())
        ? cliOptions.worldDir
        : karma::data::Resolve("client-test").string();

    platform::WindowConfig windowConfig;
    windowConfig.width = configWidth;
    windowConfig.height = configHeight;
    windowConfig.title = windowTitle;
    windowConfig.preferredVideoDriver = karma::config::ReadStringConfig({"platform.SdlVideoDriver"}, "");
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

    spdlog::trace("Starting main loop");

    ClientLoopAdapter adapter(*window, engine, serverConnector, communityBrowser, cliOptions, game);
    karma::app::EngineApp app;
    app.context().window = window.get();
    app.context().input = engine.input;
    app.context().audio = engine.audio;
    app.context().physics = engine.physics;
    app.context().overlay = engine.ui;
    engine.ecsWorld = app.context().ecsWorld;
    engine.render->setEcsWorld(app.context().ecsWorld);
    app.context().graphics = engine.render->getGraphicsDevice();
    app.context().rendererCore = engine.render->getRendererCore();
    app.setGame(&adapter);
    return app.run();
}
