#include "AppDelegate.h"

#include "GameScene.h"
#include "Strings.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <string_view>
#include <thread>

using namespace ax;

namespace {
// The size the window opens at. It can be dragged to any other; the scene lays
// itself out again against whatever it gets.
constexpr Size kWindow(1180, 780);
constexpr int  kMinWidth = 820, kMinHeight = 560;
}  // namespace

AppDelegate::AppDelegate() = default;
AppDelegate::~AppDelegate() = default;

void AppDelegate::parseArguments(int argc, char** argv) {
    GameScene::Launch& launch = GameScene::launch();

    for (int i = 1; i < argc; ++i) {
        const std::string_view flag = argv[i];
        const bool hasValue         = i + 1 < argc;

        if (flag == "--shot" && hasValue) {
            launch.screenshot = argv[++i];
        } else if (flag == "--fen" && hasValue) {
            launch.fen = argv[++i];
        } else if (flag == "--moves" && hasValue) {
            std::istringstream words(argv[++i]);
            for (std::string move; words >> move;)
                launch.moves.push_back(move);
        } else if (flag == "--pieces" && hasValue) {
            launch.pieces = argv[++i];
        } else if (flag == "--board" && hasValue) {
            launch.board = argv[++i];
        } else if (flag == "--level" && hasValue) {
            launch.level = std::atoi(argv[++i]);
        } else if (flag == "--side" && hasValue) {
            const std::string_view side = argv[++i];
            launch.side = side == "black"    ? GameSetup::Side::Black
                          : side == "random" ? GameSetup::Side::Random
                                             : GameSetup::Side::White;
        } else if (flag == "--two-players") {
            launch.opponent = GameSetup::Opponent::Human;
        } else if (flag == "--autoplay" && hasValue) {
            launch.autoplayGames = std::atoi(argv[++i]);
        } else if (flag == "--play") {
            launch.skipDialog = true;
        }
    }
}

void AppDelegate::initGfxContextAttrs() {
    GfxContextAttrs attrs = {8, 8, 8, 8, 24, 8, 0};
    RenderView::setGfxContextAttrs(attrs);
}

bool AppDelegate::applicationDidFinishLaunching() {
    auto* director   = Director::getInstance();
    auto* renderView = director->getRenderView();
    if (renderView == nullptr) {
#if (AX_TARGET_PLATFORM == AX_PLATFORM_WIN32) || (AX_TARGET_PLATFORM == AX_PLATFORM_MAC) || \
    (AX_TARGET_PLATFORM == AX_PLATFORM_LINUX)
        // Resizable: GameScene listens for the resize event, follows the
        // design resolution to the new frame size and lays itself out again.
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

    // The piece set is copied (or linked) next to the executable by CMake.
    FileUtils::getInstance()->addSearchPath("Content");

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
// GameScene::update) it stops asking and waits: glfwWaitEvents() does not
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
