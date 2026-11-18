#include "AppDelegate.h"

#include "GameScene.h"
#include "Strings.h"
#include "chess/Engine.h"

#include <charconv>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <map>
#include <optional>
#include <sstream>
#include <string_view>
#include <thread>
#include <utility>

using namespace ax;

namespace {
// The size the window opens at. It can be dragged to any other; the scene lays
// itself out again against whatever it gets.
constexpr Size kWindow(1180, 780);
constexpr int  kMinWidth = 820, kMinHeight = 560;

// Where the game keeps what it remembers between runs — the last game's
// settings and the chosen look.
//
// axmol's default for that on a Mac is the user's Documents folder, which is
// the user's and not a program's to leave files in. The place macOS means for
// this is ~/Library/Application Support/<bundle id>/. On Linux axmol already
// does the right thing (~/.config/<name>/, or $XDG_CONFIG_HOME).
//
// Nothing is brought over from the old place, on purpose: that would mean
// reaching into Documents again, which is exactly what is being stopped, and
// what is lost is five choices the next game asks for anyway.
void useProperSettingsFolder() {
#if (AX_TARGET_PLATFORM == AX_PLATFORM_MAC) && defined(CHESS_BUNDLE_ID)
    const char* home = std::getenv("HOME");
    if (home == nullptr || *home == '\0')
        return;

    std::error_code failed;
    const std::filesystem::path folder =
        std::filesystem::path(home) / "Library" / "Application Support" / CHESS_BUNDLE_ID;
    std::filesystem::create_directories(folder, failed);
    if (failed)
        return;  // stay with the default rather than have nowhere to write

    FileUtils::getInstance()->setWritablePath(folder.string() + "/");
#endif
}

}  // namespace

AppDelegate::AppDelegate() = default;
AppDelegate::~AppDelegate() = default;

void AppDelegate::parseArguments(int argc, char** argv) {
    GameScene::Launch launch;

    // A whole number; or a word in the log and nothing, so that the option
    // stays as it was: "--level abc" must not quietly become level 0.
    const auto number = [](std::string_view flag, std::string_view text) -> std::optional<int> {
        int value = 0;
        const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
        if (error != std::errc() || end != text.data() + text.size()) {
            AXLOGW("{} ignored: \"{}\" is not a whole number", flag, text);
            return std::nullopt;
        }
        return value;
    };

    // The options that take a value, and what each does with it; then those
    // that are a word by themselves. What they mean is said at
    // GameScene::Launch.
    using Value = std::string_view;
    const std::map<std::string_view, std::function<void(Value)>> valued = {
        {"--shot", [&](Value v) { launch.screenshot = v; }},
        {"--fen", [&](Value v) { launch.fen = v; }},
        {"--moves",
         [&](Value v) {
             std::istringstream words{std::string(v)};
             for (std::string move; words >> move;)
                 launch.moves.push_back(move);
         }},
        {"--pieces", [&](Value v) { launch.pieces = v; }},
        {"--board", [&](Value v) { launch.board = v; }},
        {"--level",
         [&](Value v) {
             if (const auto n = number("--level", v))
                 launch.level = n;
         }},
        {"--elo",
         [&](Value v) {
             if (const auto n = number("--elo", v))
                 launch.elo = n;
         }},
        {"--side",
         [&](Value v) {
             launch.side = v == "black"    ? GameSetup::Side::Black
                           : v == "random" ? GameSetup::Side::Random
                                           : GameSetup::Side::White;
         }},
        {"--autoplay", [&](Value v) { launch.autoplayGames = number("--autoplay", v).value_or(launch.autoplayGames); }},
        {"--edit", [&](Value v) { launch.editFen = v; }},
        {"--puzzle", [&](Value v) { launch.puzzle = v; }},
    };
    const std::map<std::string_view, std::function<void()>> switches = {
        {"--two-players", [&] { launch.opponent = GameSetup::Opponent::Human; }},
        {"--puzzles", [&] { launch.puzzles = true; }},
        {"--play", [&] { launch.skipDialog = true; }},
    };

    for (int i = 1; i < argc; ++i) {
        const std::string_view flag = argv[i];
        if (const auto option = valued.find(flag); option != valued.end()) {
            if (i + 1 < argc)
                option->second(argv[++i]);
        } else if (const auto option = switches.find(flag); option != switches.end()) {
            option->second();
        }
        // Anything else is left alone: the system adds arguments of its own
        // (macOS: -psn_..., -NSDocumentRevisionsDebugMode).
    }

    GameScene::setLaunch(std::move(launch));
}

void AppDelegate::initGfxContextAttrs() {
    GfxContextAttrs attrs = {8, 8, 8, 8, 24, 8, 0};
    RenderView::setGfxContextAttrs(attrs);
}

bool AppDelegate::applicationDidFinishLaunching() {
    // Before anything reads a setting.
    useProperSettingsFolder();

    auto* director   = Director::getInstance();
    auto* renderView = director->getRenderView();
    if (renderView == nullptr) {
#if (AX_TARGET_PLATFORM == AX_PLATFORM_WIN32) || (AX_TARGET_PLATFORM == AX_PLATFORM_MAC) || \
    (AX_TARGET_PLATFORM == AX_PLATFORM_LINUX)
        // Resizable: each scene follows the window (desktop::followWindow),
        // sets the design resolution to the new frame size and lays itself
        // out again.
        auto* window = RenderViewImpl::createWithRect(text::kTitle, ax::Rect(0, 0, kWindow.width, kWindow.height),
                                                      /*frameZoomFactor=*/1.0f, /*resizable=*/true);
        // Below this the panel and the board no longer both fit.
        window->setWindowSizeLimits(kMinWidth, kMinHeight);
        renderView = window;
#else
        renderView = RenderViewImpl::create(text::kTitle);
#endif
        director->setRenderView(renderView);
    }

    director->setAnimationInterval(1.0f / 60);
    renderView->setDesignResolutionSize(kWindow.width, kWindow.height, ResolutionPolicy::SHOW_ALL);

    // Text and the soft edges of drawn shapes are made for the screen they
    // are on. A system-font Label bakes a bitmap once, at the point size it
    // was given; on a 2x display that bitmap would be stretched over twice as
    // many pixels, which is exactly what a blurry glyph looks like. With the
    // content scale factor set to the display's pixel ratio the bitmap is
    // rasterised at twice the size and measured back down, so it lands on
    // screen the same size with twice the detail. On a 1x display the ratio is
    // 1 and this line does nothing.
    //
    // It has to come before any Label is created, which is why it is here and
    // not in the scene.
    director->setContentScaleFactor(Device::getPixelRatio());

    // Where the piece sets are. On a Mac they are in the bundle's Resources,
    // which is already searched; elsewhere CMake links the folder next to the
    // executable as Content/. By its full path: a relative one would be looked
    // for in whatever directory the game happened to be started from.
    if (const std::string here = chess::executableDirectory(); !here.empty())
        FileUtils::getInstance()->addSearchPath(here + "/Content");

    director->runWithScene(utils::createInstance<GameScene>());
    return true;
}

// axmol's own loop, Application::run(), is a game's loop: sixty times a second
// it draws a frame, asks the system for events and sleeps until the next one is
// due. Director::stopAnimation() takes the frame out of that, and nothing else:
// the asking and the sleeping go on, sixty wake-ups a second, for as long as
// the window is open. On a board where nothing moves for minutes at a time
// that is the whole of the program's CPU use, and it is all waste.
//
// So this loop has two gears. While the director is animating it is axmol's
// loop, line for line. Once the scene has stopped the animation (see
// Wakefulness.h) it stops asking and waits: glfwWaitEvents() does not
// return until the system has something for this window — a key, the mouse, a
// resize, a close. The process is then blocked in the kernel and uses no CPU
// at all. Whatever arrives is delivered through the same GLFW callbacks as
// ever; if it matters, the scene restarts the animation from inside one of
// them and the next turn of the loop draws.
//
// Nothing but the window can end the wait, which is fine because of a rule
// the scene keeps: it never stops the animation while something is still
// owed to it from elsewhere — an engine's move, an analysis, a timer.
int AppDelegate::runUntilClosed() {
#if (AX_TARGET_PLATFORM == AX_PLATFORM_MAC) || (AX_TARGET_PLATFORM == AX_PLATFORM_LINUX)
    initGfxContextAttrs();
    if (!applicationDidFinishLaunching())
        return 1;

    using Clock          = std::chrono::steady_clock;
    const bool   logging = std::getenv("CHESS_IDLE_LOG") != nullptr;
    unsigned long frames = 0;

    auto* director = Director::getInstance();
    auto* view     = director->getRenderView();
    view->retain();  // the director lets go of it while shutting down

    while (!view->windowShouldClose()) {
        const auto began = Clock::now();

        // Draws a frame if the director is animating. Called either way: it
        // also carries out a requested shutdown.
        director->mainLoop();
        view->pollEvents();

        if (!director->isValid()) {
            // Asked once more above before going to sleep, so an event that
            // came in during the last frame is not left waiting for the next.
            if (view->windowShouldClose())
                break;
            if (logging && frames != 0)
                std::fprintf(stderr, "idle: asleep after %lu frames\n", frames);
            frames = 0;
            glfwWaitEvents();
            continue;
        }
        ++frames;

        const auto frame = std::chrono::duration_cast<Clock::duration>(
            std::chrono::duration<float>(director->getAnimationInterval()));
        const auto spent = Clock::now() - began;
        if (spent < frame)
            std::this_thread::sleep_for(frame - spent);
        else
            std::this_thread::yield();
    }

    // As in Application::run(): end() only asks, and the frame after it is the
    // one that releases everything.
    if (view->isGfxContextReady()) {
        director->end();
        director->mainLoop();
    }
    view->release();
    return 0;
#else
    return Application::run();
#endif
}

void AppDelegate::applicationDidEnterBackground() {
    Director::getInstance()->stopAnimation();
}

void AppDelegate::applicationWillEnterForeground() {
    Director::getInstance()->startAnimation();
}
