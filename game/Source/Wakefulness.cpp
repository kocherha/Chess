#include "Wakefulness.h"

#include "axmol.h"

#include <cstdio>
#include <cstdlib>

using ax::Director;

void Wakefulness::update(float delta, std::initializer_list<Reason> reasons) {
    const char* waiting = nullptr;
    for (const Reason& reason : reasons)
        if (reason.holds) {
            waiting = reason.why;
            break;
        }

    static const bool logging = std::getenv("CHESS_IDLE_LOG") != nullptr;
    if (logging && waiting != _because)
        std::fprintf(stderr, "idle: %s\n", waiting != nullptr ? waiting : "nothing left to wait for");
    _because = waiting;

    _quiet = waiting != nullptr ? 0.0f : _quiet + delta;
    if (_quiet > kSleepAfter) {
        _quiet = 0.0f;
        Director::getInstance()->stopAnimation();
    }
}

void Wakefulness::keepAwake() {
    _quiet = 0.0f;
    // The director is asked rather than remembered: it is also stopped and
    // started when the app goes to the background and comes back.
    auto* director = Director::getInstance();
    if (!director->isValid())
        director->startAnimation();
}
