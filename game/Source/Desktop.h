// What both scenes ask of the desktop around the window: the clipboard, the
// window's own events, and a picture of the window for --shot.
#pragma once

#include "axmol.h"

#include <functional>
#include <string>

namespace desktop {

// The system clipboard, as text. On a platform without one, nothing is put
// and nothing comes back.
void        copy(const std::string& text);
std::string paste();

// The system's half of "something happened", for a scene. Moved to another
// display or brought back to the front, a window that has stopped drawing
// can show stale pixels: `wake` buys it a few frames. Resized: it wakes, and
// if the window has a size at all, the scene is laid out again for it
// (`resized`) — design resolution = frame size, so the scene is never scaled
// to fit: a larger window shows a larger board rather than the same picture
// stretched, and the text stays crisp. The close button, Cmd+Q, the window
// manager: `closing` may set its flag to false to keep the window open.
//
// The listeners belong to `scene` and go with it. Off the desktop, nothing.
struct WindowHooks {
    std::function<void()>              wake;
    std::function<void()>              resized;
    std::function<void(bool& closing)> closing;
};
void followWindow(ax::Node* scene, WindowHooks hooks);

// The mouse, for a scene that decides itself whom each event belongs to —
// an open dialog, a button, the board — in one place, instead of every node
// listening and the first to hear a click taking it. The scene's handlers
// get the point in world coordinates; `down` is told whether it was the
// right button (the left one otherwise; the middle one is not passed on),
// `up` comes for the left button only. `wheel`, if given, gets the wheel's
// or trackpad's movement as the system reports it: negative is up.
struct MouseHooks {
    std::function<void(const ax::Vec2& point, bool right)>   down;
    std::function<void(const ax::Vec2& point)>               up;
    std::function<void(const ax::Vec2& point)>               move;
    std::function<void(const ax::Vec2& point, float amount)> wheel;
};
void followMouse(ax::Node* scene, MouseHooks hooks);

// Saves a picture of the window to `path` and quits — once the screen has
// stopped changing: `busy` has said no for a little while (an engine
// answering, a piece landing, a bar settling). Looked at four times a second,
// and taken regardless after two minutes. Runs on `host`'s schedule.
void screenshotWhenSettled(ax::Node* host, const std::string& path, std::function<bool()> busy);

}  // namespace desktop
