// How the window is shared between the board and the panel — the same in
// both modes, so that switching between a game and the problems does not
// make the panel jump.
//
// Nothing is a fixed pixel position. The panel takes a share of the width
// between two limits; what is left is the board's, with whatever stands
// round it in that mode (the name strips and the evaluation bar in a game,
// the trays of pieces in the problems).
#pragma once

#include <algorithm>
#include <cmath>

namespace layout {

// Kept clear round the board and what stands with it.
inline constexpr float kMargin = 26.0f;

struct Split {
    float panelWidth = 0.0f;  // the panel, along the right edge, the full height
    float areaWidth  = 0.0f;  // the rest, to the left of it
};

inline Split split(float screenWidth) {
    Split out;
    out.panelWidth = std::clamp(std::floor(screenWidth * 0.27f), 300.0f, 380.0f);
    out.areaWidth  = screenWidth - out.panelWidth;
    return out;
}

// The side of the largest board that fits in `width` × `height`: a whole
// number of pixels per square, so that every edge between two squares lands
// on a pixel boundary, and never too small to play on.
inline float boardSide(float width, float height) {
    const float side = std::min(width, height);
    return std::max(160.0f, std::floor(side / 8.0f) * 8.0f);
}

}  // namespace layout
