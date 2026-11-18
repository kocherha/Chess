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
