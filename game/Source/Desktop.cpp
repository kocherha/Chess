#include "Desktop.h"

#include <memory>
#include <string_view>

using namespace ax;

namespace desktop {

void copy(const std::string& text) {
#if defined(AX_PLATFORM_PC)
    glfwSetClipboardString(nullptr, text.c_str());
#else
    (void)text;
#endif
}

std::string paste() {
#if defined(AX_PLATFORM_PC)
    if (const char* text = glfwGetClipboardString(nullptr))
        return text;
#endif
    return {};
}

void followWindow(Node* scene, WindowHooks hooks) {
#if defined(AX_PLATFORM_PC)
    // Scene-graph priority, not addCustomEventListener: the latter registers
    // with the dispatcher itself and would outlive the scene.
    auto* dispatcher = scene->getEventDispatcher();
    const auto when  = [&](std::string_view event, std::function<void(EventCustom*)> what) {
        dispatcher->addEventListenerWithSceneGraphPriority(EventListenerCustom::create(event, std::move(what)), scene);
    };
    when(RenderViewImpl::EVENT_WINDOW_POSITIONED, [wake = hooks.wake](EventCustom*) { wake(); });
    when(RenderViewImpl::EVENT_WINDOW_FOCUSED, [wake = hooks.wake](EventCustom*) { wake(); });
    when(RenderViewImpl::EVENT_WINDOW_RESIZED, [wake = hooks.wake, resized = hooks.resized](EventCustom*) {
        wake();
        auto* view = Director::getInstance()->getRenderView();
        if (view == nullptr)
            return;
        const Vec2 frame = view->getFrameSize();
        if (frame.x < 1.0f || frame.y < 1.0f)
            return;  // minimised: nothing to lay out for
        view->setDesignResolutionSize(frame.x, frame.y, ResolutionPolicy::SHOW_ALL);
        resized();
    });
    when(RenderViewImpl::EVENT_WINDOW_CLOSE, [closing = hooks.closing](EventCustom* event) {
        if (auto* flag = static_cast<bool*>(event->getUserData()))
            closing(*flag);
    });
#else
    (void)scene;
    (void)hooks;
#endif
}

void followMouse(Node* scene, MouseHooks hooks) {
    auto* mouse        = EventListenerMouse::create();
    mouse->onMouseDown = [down = hooks.down](EventMouse* event) {
        const auto button = event->getMouseButton();
        if (button == EventMouse::MouseButton::BUTTON_LEFT || button == EventMouse::MouseButton::BUTTON_RIGHT)
            down(event->getLocation(), button == EventMouse::MouseButton::BUTTON_RIGHT);
        return true;
    };
    mouse->onMouseUp = [up = hooks.up](EventMouse* event) {
        if (event->getMouseButton() == EventMouse::MouseButton::BUTTON_LEFT)
            up(event->getLocation());
        return true;
    };
    mouse->onMouseMove = [move = hooks.move](EventMouse* event) {
        move(event->getLocation());
        return true;
    };
    if (hooks.wheel) {
        mouse->onMouseScroll = [wheel = hooks.wheel](EventMouse* event) {
            wheel(event->getLocation(), event->getScrollY());
            return true;
        };
    }
    // Scene-graph priority: the listener goes with the scene.
    scene->getEventDispatcher()->addEventListenerWithSceneGraphPriority(mouse, scene);
}

void screenshotWhenSettled(Node* host, const std::string& path, std::function<bool()> busy) {
    // How long it has waited, and for how long nothing has been going on.
    struct Clock {
        float waited = 0.0f, settled = 0.0f;
    };
    auto clock = std::make_shared<Clock>();

    host->schedule(
        [host, path, busy = std::move(busy), clock](float delta) {
            clock->waited += delta;
            clock->settled = busy() ? 0.0f : clock->settled + delta;
            if (clock->waited < 1.5f || (clock->settled < 0.6f && clock->waited < 120.0f))
                return;

            const std::string target = path;  // this callback is unscheduled next
            host->unschedule("screenshot");
            utils::captureScreen(
                [](bool ok, std::string_view written) {
                    AXLOGI("screenshot {}: {}", ok ? "written" : "FAILED", written);
                    Director::getInstance()->end();
                },
                target);
        },
        0.25f, "screenshot");
}

}  // namespace desktop
