// Two small things every part of the interface needs from a label.
#pragma once

#include "axmol.h"

#include <string>

namespace typeset {

// A system-font label in the interface's typeface.
ax::Label* label(const std::string& text, float points, const ax::Color3B& colour);

// Makes a label's strokes heavier, for a title or a sign that has to carry at
// a small size. The label is drawn a second time a fraction of a point to the
// right of itself, which is all a bold system font amounts to at these sizes.
//
// axmol's own Label::enableBold() does exactly this and then also asks for
// wider letter spacing — which a system-font label cannot do, and says so in
// the log every time it is asked.
void embolden(ax::Label* label);

// How far that second drawing is from the first. A label that has to sit
// dead centre on something small is moved back by half of it.
inline constexpr float kBoldShift = 0.9f;

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
