// What both scenes ask of the desktop around the window: the clipboard, and a
// picture of the window for --shot.
#pragma once

#include "axmol.h"

#include <functional>
#include <string>

namespace desktop {

// The system clipboard, as text. On a platform without one, nothing is put
// and nothing comes back.
void        copy(const std::string& text);
std::string paste();

// Saves a picture of the window to `path` and quits — once the screen has
// stopped changing: `busy` has said no for a little while (an engine
// answering, a piece landing, a bar settling). Looked at four times a second,
// and taken regardless after two minutes. Runs on `host`'s schedule.
void screenshotWhenSettled(ax::Node* host, const std::string& path, std::function<bool()> busy);

}  // namespace desktop
