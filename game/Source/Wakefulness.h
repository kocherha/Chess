#pragma once

#include <initializer_list>

// When a screen stops drawing.
//
// Nothing on either screen moves on its own. Between a click and the next one
// — which, in chess, can be minutes — every frame would draw exactly the same
// pixels as the last. So after half a second of quiet the game stops:
// Director::stopAnimation() makes mainLoop() skip the frame, and the main loop
// (AppDelegate::runUntilClosed) then blocks until the system has an event for
// the window. Asleep, the program uses no CPU at all.
//
// Input still arrives, because mouse, keyboard and window events are
// dispatched straight from the GLFW callbacks, not from inside a frame; the
// ones that change something on screen call keepAwake(). What does not arrive
// that way is everything a scene collects in update() — an engine's answer
// from another thread, a timer running down — and update() only runs while
// frames do. So a scene never goes to sleep while it is waiting for any of
// that: nothing would wake it. The list of what it waits for is the scene's,
// and it has to be complete.
//
// CHESS_IDLE_LOG=1 says what is keeping the game awake, whenever that
// changes: the way to find out why it uses the processor when it looks as if
// it should not.
class Wakefulness {
public:
    static constexpr float kSleepAfter = 0.5f;  // seconds of quiet

    struct Reason {
        bool        holds;
        const char* why;  // for the log: "the opponent is thinking"
    };

    // Once a frame, with every reason the scene has to keep drawing. After
    // kSleepAfter with none of them holding, the drawing stops.
    void update(float delta, std::initializer_list<Reason> reasons);

    // Something has changed on screen, or is about to: draw, and start the
    // half second again.
    void keepAwake();

private:
    float       _quiet   = 0.0f;     // seconds since anything last happened
    const char* _because = nullptr;  // what the last frame found to wait for, if anything
};
