// Two small things every part of the interface needs from a label.
#pragma once

#include "axmol.h"

#include <string>

namespace typeset {

// A system-font label in the interface's typeface.
ax::Label* label(const std::string& text, float points, const ax::Color3B& colour);

// Breaks `text` into lines no wider than `maxWidth` when set in `points`, and
// returns it with the line breaks in place.
//
// A system-font label can wrap by itself — give it a width and it will — but
// how it does that is each platform's own text code, and they do not agree:
// the Linux one drops words around a multi-byte character such as a dash.
// Measuring the words ourselves and breaking where they stop fitting gives the
// same lines everywhere.
std::string wrap(const std::string& text, float points, float maxWidth);

}  // namespace typeset
