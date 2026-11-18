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

// The typeface of the signs on the board — "!", "#", "½" — which are set the
// way a chess book sets them. Times New Roman is on every Mac; elsewhere the
// name resolves to the system's own Times (Liberation Serif, Nimbus Roman).
inline constexpr const char* kSerif = "Times New Roman";

inline ax::Color4F rgb(int r, int g, int b, float a = 1.0f) {
    return ax::Color4F(static_cast<float>(r) / 255.0f, static_cast<float>(g) / 255.0f,
                       static_cast<float>(b) / 255.0f, a);
}

// The same colour for what takes a Color3B — a label's text — and has no
// transparency.
inline ax::Color3B toColor3B(const ax::Color4F& colour) {
    return ax::Color3B(static_cast<uint8_t>(colour.r * 255.0f), static_cast<uint8_t>(colour.g * 255.0f),
                       static_cast<uint8_t>(colour.b * 255.0f));
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
inline const ax::Color3B kBad(236, 112, 100);  // what is wrong: a move, a position from the editor

// How the status card on either panel feels: neutral, good news, a warning,
// or — in the problems — a wrong move. Its headline is written in one colour
// and its border drawn in another.
enum class Tone { Neutral, Good, Warning, Bad };

inline ax::Color3B toneInk(Tone tone) {
    switch (tone) {
    case Tone::Good:    return kGood;
    case Tone::Warning: return kWarn;
    case Tone::Bad:     return kBad;
    case Tone::Neutral: break;
    }
    return kInk;
}

inline ax::Color4F toneBorder(Tone tone) {
    switch (tone) {
    case Tone::Good:    return kAccent;
    case Tone::Warning: return rgb(170, 130, 70);
    case Tone::Bad:     return rgb(176, 72, 64);
    case Tone::Neutral: break;
    }
    return kLine;
}

// The colour a move's mark is written in in the list of moves: the five
// colours other chess programs have taught people to expect, green-blue for
// the finds, yellow through red for what went wrong. On the board the same marks are
// printed in ink like every other badge; see kBadgePaper below.
inline ax::Color4F markColour(chess::Mark mark) {
    switch (mark) {
    case chess::Mark::Brilliant:  return rgb(38, 194, 163);
    case chess::Mark::Great:      return rgb(104, 160, 214);
    case chess::Mark::Inaccuracy: return rgb(230, 172, 36);
    case chess::Mark::Mistake:    return rgb(236, 146, 54);
    case chess::Mark::Blunder:    return rgb(232, 76, 66);
    case chess::Mark::None:       break;
    }
    return rgb(kDim.r, kDim.g, kDim.b);
}

// A badge on the board — what a move was worth, how the game stands for a
// king — is printed rather than painted: a disc of book paper, ruled round in
// ink, with the sign in ink on it. One pair of colours for all of them; which
// sign it is says the rest, as it does on a printed page.
inline const ax::Color4F kBadgePaper = rgb(246, 238, 219);
inline const ax::Color4F kBadgeInk   = rgb(38, 28, 20);

// The board's own colours, and the marks that depend on them, are a choice the
// player makes: see Look.h. The hint arrow is the same on every board.
inline const ax::Color4F kHint = rgb(66, 140, 224, 0.82f);

// The position editor's red: a square that makes the position impossible.
inline const ax::Color4F kProblem = rgb(214, 48, 40, 0.58f);

}  // namespace theme
