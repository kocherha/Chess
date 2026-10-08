// The colours and the typeface of everything around the board — the
// background, the panel, the buttons, the dialogs — in one place, so the look
// of the interface is a decision made once rather than a number repeated in
// six files. What the board and the pieces look like is the player's choice
// and lives in Look.h.
#pragma once

#include "chess/Judge.h"

#include "axmol.h"

namespace theme {

// Present on every desktop this builds for, and it has the Cyrillic the
// interface is written in.
inline constexpr const char* kFont = "Arial";

inline ax::Color4F rgb(int r, int g, int b, float a = 1.0f) {
    return ax::Color4F(static_cast<float>(r) / 255.0f, static_cast<float>(g) / 255.0f,
                       static_cast<float>(b) / 255.0f, a);
}

// The room.
inline const ax::Color4F kBackground = rgb(24, 26, 31);
inline const ax::Color4F kPanel      = rgb(33, 36, 43);
inline const ax::Color4F kCard       = rgb(42, 46, 55);
inline const ax::Color4F kCardHover  = rgb(52, 57, 68);
inline const ax::Color4F kCardPress  = rgb(36, 40, 48);
inline const ax::Color4F kLine       = rgb(58, 63, 75);
// The scroll bar's thumb under the pointer, and while it is being dragged.
inline const ax::Color4F kThumbHot   = rgb(96, 103, 120);
inline const ax::Color4F kThumbHeld  = rgb(132, 140, 160);
inline const ax::Color4F kScrim      = rgb(10, 11, 14, 0.72f);
// What captured pieces lie on: a middle grey, because half of them are black
// and half are white and both have to show against it.
inline const ax::Color4F kTray       = rgb(134, 139, 152);

// The one colour that means "this is the thing to press".
inline const ax::Color4F kAccent      = rgb(118, 168, 72);
inline const ax::Color4F kAccentHover = rgb(134, 186, 84);
inline const ax::Color4F kAccentPress = rgb(100, 146, 60);

inline const ax::Color3B kInk(236, 236, 240);
inline const ax::Color3B kDim(150, 155, 168);
inline const ax::Color3B kFaint(104, 109, 122);
inline const ax::Color3B kOnAccent(255, 255, 255);
inline const ax::Color3B kGood(150, 200, 100);
inline const ax::Color3B kWarn(232, 178, 92);

// The colour a move's mark is written in. The same five everywhere a mark
// appears, and the ones other chess programs have taught people to expect:
// green-blue for the finds, yellow through red for what went wrong. Each is
// deep enough for a white sign to read on it, and for the disc to stand out
// on a board without an outline — the yellow one against the yellow of the
// last move's squares in particular.
inline ax::Color4F markColour(chess::Mark mark) {
    switch (mark) {
    case chess::Mark::Brilliant:  return rgb(38, 194, 163);
    case chess::Mark::Great:      return rgb(104, 160, 214);
    case chess::Mark::Inaccuracy: return rgb(230, 172, 36);
    case chess::Mark::Mistake:    return rgb(236, 146, 54);
    case chess::Mark::Blunder:    return rgb(232, 76, 66);
    case chess::Mark::None:       break;
    }
    return rgb(150, 155, 168);
}

// What a badge on a king is painted in: the king is attacked or mated, the
// king has won, and everything that is neither — a draw, a resignation.
inline const ax::Color4F kSignDanger = rgb(226, 72, 62);
inline const ax::Color4F kSignWon    = rgb(104, 160, 62);
inline const ax::Color4F kSignQuiet  = rgb(112, 118, 134);

// The board's own colours, and the marks that depend on them, are a choice the
// player makes: see Look.h. The hint arrow is the same on every board.
inline const ax::Color4F kHint = rgb(66, 140, 224, 0.82f);

}  // namespace theme
